#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Airdrops
// TODO: Sprint meter (regenerates slowly, allows for short sprints)

#define CHUNKSIZE 128
#define cameraZoom 1.0f
#define MAXCHUNKS 25
#define TILESONSCREEN 20
#define WIDECHUNKS 50 
#define FPS 120
#define SPEED 9
#define MAXZOMBIES 1000
// This does nothing rn
#define NUMSPAWNLOCATIONS 4
#define ZOMBIESPEED 7
// Shotgun Cooldown 0.5s
#define SGCD 0.5
// #define debug true

enum {UP, DOWN, LEFT, RIGHT}; // Directions
enum {PLAYING, GAMEOVER, START, PAUSED}; // Game screens

struct player
{
  Vector2 pos;
  int weapon;
  int kills;
  int money;
  int health;
};

struct mapChunk
{
  char tiles[CHUNKSIZE][CHUNKSIZE];
  Vector2 pos;
};

static int randoms[CHUNKSIZE][CHUNKSIZE];

static struct mapChunk activeChunks[4];
// 2 Variables to store what side of the current chunk has active loaded chunks
static char activeChunkExistsX; // -1->Left; 1->Right
static char activeChunkExistsY; // -1->Top; 1->Below
static struct mapChunk chunks[MAXCHUNKS] = { 0 };
static unsigned int openSlots[MAXCHUNKS] = { -1 };

int saveActiveChunk(int slot);
int loadChunk(int slot, int xPos, int yPos);

static int mouseMode = 2;
static int gamePaused = 2;
static int playerDead = 0;
static int sW = 1280;
static int sH = 720;

static unsigned int frameCount = 0;
static int facing = 0; // Direction the player is facing

static Vector2 zombies[MAXZOMBIES];
static Vector2 spawnLocations[NUMSPAWNLOCATIONS];
static int spawnLocationsI;
static int spawnAt;
static float shotgunCooldown = 0;

static Vector2 normalisedMouse;
static Vector2 scheduledMovement;
static struct player player = { 0 };
static Camera2D mainCam = { 0 };

float radianConvert(float angle);
int fullscreenAdjust();
int xorShift32(int state);
int findChunk(int length, struct mapChunk chunks[length], int xPos, int yPos, int flags);
char *getTile(Vector2 pos);
// int spiralFindTile(Vector2 pos, int *x, int *y, int *activeChunk, char match);  // Not implemented
int toggleState(int *var);

int startScreen();
int setupGame();
int handleControls();
int tick();
int drawGame();
int drawUI();
int drawScreen(int screen);

Texture2D grassTex;
Texture2D manLeft;
Texture2D manLeftWalk[2];
Texture2D manRight;
Texture2D manRightWalk[2];

Texture2D zombieLeftWalk[2];
Texture2D zombieRightWalk[2];

int main(int argc, char *argv[])
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);    // Window configuration flags
  InitWindow(1280, 720, "Hoard avoidance");
  SetTargetFPS(FPS);
  #ifndef debug
  fullscreenAdjust();
  #endif /* ifndef debug */

  // Load textures
  grassTex = LoadTexture("grass.png");
  manLeft = LoadTexture("ManLeft.png");
  manLeftWalk[0] = LoadTexture("ManLeftWalk1.png");
  manLeftWalk[1] = LoadTexture("ManLeftWalk2.png");
  manRight = LoadTexture("ManRight.png");
  manRightWalk[0] = LoadTexture("ManRightWalk1.png");
  manRightWalk[1] = LoadTexture("ManRightWalk2.png");

  zombieLeftWalk[0] = LoadTexture("Zombie2LeftWalk1.png");
  zombieLeftWalk[1] = LoadTexture("Zombie2LeftWalk2.png");
  zombieRightWalk[0] = LoadTexture("Zombie2RightWalk1.png");
  zombieRightWalk[1] = LoadTexture("Zombie2RightWalk2.png");

  // Pregenerate the random textures so that rendering is faster
  for (int x = 0; x < CHUNKSIZE; ++x)
    for (int y = 0; y < CHUNKSIZE; ++y)
      randoms[x][y] = xorShift32(xorShift32((int)(x) ^ 1455093647) ^ xorShift32((int)(y) ^ 1455093647));

  // Set up game variables
  setupGame();
  BeginDrawing();
  drawScreen(START);
  EndDrawing();

  // Main loop
  while (!WindowShouldClose())
  {
    // Take keyboard inputs
    handleControls();
    // Update game variables
    if (!gamePaused) tick();

    if (gamePaused == 2)
    {
      startScreen();
      continue;
    }

    BeginDrawing();
      ClearBackground(RAYWHITE);
      BeginMode2D(mainCam);
        // Draw the game
        drawGame();
      EndMode2D();
      // Draw UI stuff
      drawUI();
    EndDrawing();
  }

  return 0;
}


int setupGame()
{
  // Reset player
  playerDead = 0;
  player.pos.x = 10.f;
  player.pos.y = 10.f;
  player.weapon = 1;
  player.kills = 0;
  player.money = 50;
  // Reset camera
  mainCam.target = (Vector2){ 0.f, 0.f };
  mainCam.zoom = 1.f;
  mainCam.offset = (Vector2){ 0.f, 0.f };
  mainCam.rotation = 0.0f;
  // Reset chunks
  memset(chunks, 0, sizeof(chunks));
  memset(openSlots, -1, sizeof(openSlots));
  memset(activeChunks, 0, sizeof(activeChunks));
  activeChunks[0].pos = (Vector2){ -1.f, -1.f };
  activeChunks[1].pos = (Vector2){ 0.f, -1.f };
  activeChunks[2].pos = (Vector2){ -1.f, 0.f };
  activeChunks[3].pos = (Vector2){ 0.f, 0.f };
  activeChunkExistsX = -1;
  activeChunkExistsY = -1;
  // Clear out the zombies
  SetRandomSeed(69);
  Vector2 v;
  for (int i = 0; i < MAXZOMBIES; ++i)
    zombies[i] = (Vector2){ 0, 0 };
  for (int i = 0; i < NUMSPAWNLOCATIONS; ++i)
    spawnLocations[i] = (Vector2){ 0, 0 };
  spawnLocationsI = 0;

  return 0;
}


int toggleState(int *var)
{
  *var = !*var;
  return !*var;
}

int fullscreenAdjust()
{
  int display = GetCurrentMonitor();
  if (IsWindowFullscreen())
  {
    sW = 1280;
    sH = 720;
    SetWindowSize(sW, sH);
    ToggleFullscreen();
  } else {
    sW = GetMonitorWidth(display);
    sH = GetMonitorHeight(display);
    ToggleFullscreen();
    SetWindowSize(sW, sH);
  }
  return 0;
}


int handleControls()
{
  // Mouse Mode
  if (IsKeyPressed(KEY_M)) mouseMode = 2;
  if (IsKeyPressed(KEY_K)) toggleState(&mouseMode);

  // Fullscreening
  if (IsKeyPressed(KEY_F11)) fullscreenAdjust();

  // Pausing
  if (IsKeyPressed(KEY_P) && !playerDead) toggleState(&gamePaused);

  // Restarting
  if (playerDead && IsKeyPressed(KEY_ENTER))
  {
    gamePaused = 2;
    setupGame();
  }

  // Handle movement; peform collision check with tile
  scheduledMovement = (Vector2){ 0, 0 };
  if (IsKeyDown(KEY_W))
    scheduledMovement.y -= (float) SPEED / FPS;
  if (IsKeyDown(KEY_S))
    scheduledMovement.y += (float) SPEED / FPS;
  if (IsKeyDown(KEY_A))
    scheduledMovement.x -= (float) SPEED / FPS;
  if (IsKeyDown(KEY_D))
    scheduledMovement.x += (float) SPEED / FPS;

  // Set player animation direction
  if (scheduledMovement.x > 0) facing = 1;
  if (scheduledMovement.x < 0) facing = 0;

  // Player direction
  if (!mouseMode)
  {
    if (scheduledMovement.x != 0.f || scheduledMovement.y != 0.f)
      normalisedMouse = scheduledMovement;
  }
  else if (mouseMode == 1)
  {
    if (scheduledMovement.x != 0.f || scheduledMovement.y != 0.f)
    normalisedMouse = Vector2Scale(scheduledMovement, -1.f);
  }
  else normalisedMouse = Vector2Add(GetMousePosition(), (Vector2){ -0.5 * sW, -0.5 * sH });

  float angle;
  Vector2 zom;
  int chunk, chunkx, chunky;

  if ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsKeyDown(KEY_SPACE)) && shotgunCooldown > SGCD)
  {
    shotgunCooldown = 0.f;
    // Check for all zombie in 360 range then refine
    for (int i = 0; i < MAXZOMBIES; ++i)
      if (zombies[i].x != 0)
        if (Vector2Distance(zombies[i], player.pos) <= 3)
        {
          // Check for all the zombies within 45 degrees of aimed direction
          angle = Vector2Angle(normalisedMouse, Vector2Subtract(zombies[i], player.pos));
          if (angle > -0.785398 && angle < 0.785398)
          {
            // Delete the zombie and set the tile at its location to solid
            *getTile(zombies[i]) = 1;
            spawnLocations[spawnLocationsI] = zombies[i];
            spawnLocationsI = ++spawnLocationsI >= NUMSPAWNLOCATIONS ? 0 : spawnLocationsI;
            zombies[i] = (Vector2){ 0, 0 };
            // Increment player kills
            player.kills++;
          }
        }
  }


  #ifdef debug
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsKeyDown(KEY_SPACE))
    // normalisedMouse
    *getTile(Vector2Add(Vector2Scale(normalisedMouse, (float) TILESONSCREEN / sH), player.pos)) = 1;
  #endif /* ifdef debug */
  return 0;
}


int tick()
{
  // Cooldown
  shotgunCooldown += 1.f / FPS;
  // Animate
  frameCount++;
  // Move zombies towards player
  float distance;
  int zombiesToPlace = GetRandomValue(1, FPS) / FPS;
  // Try to spawn 4 zombies every half second
  int tileZombies = (GetRandomValue(1, FPS) / FPS) * 4;
  for (int i = 0; i < MAXZOMBIES; ++i)
    if (zombies[i].x != 0)
    {
      distance = Vector2Distance(player.pos, zombies[i]);
      // Check if player is dead
      if (distance < 0.5f)
      {
        playerDead = 1;
        gamePaused = 1;
      }
      // Vector2Add(player.pos, (Vector2){ GetRandomValue(-5, 5), GetRandomValue(-5, 5)})
      zombies[i] = Vector2Lerp(zombies[i], player.pos, (float) ZOMBIESPEED / FPS / distance);
      // Really inefficent but check for collisions with all other zombies
      // int touching = 0;
      for (int j = 0; j < MAXZOMBIES; ++j)
      {
        if (i == j) continue;
        float xSep = zombies[i].x - zombies[j].x;
        float ySep = zombies[i].y - zombies[j].y;
        if (xSep > 0.15 || xSep < -0.15 || ySep > 0.15 || xSep < -0.15) continue;
        float distance = Vector2Distance(zombies[i], zombies[j]);
        if (distance > 0.3f) continue;
        zombies[i] = Vector2Lerp(zombies[j], zombies[i], 2);
        // if (touching++ >= 5) break;
      }
      // printf("%d\n", touching);
    }
    else if (zombiesToPlace)
    {
      zombies[i] = Vector2Add(player.pos, Vector2Rotate((Vector2){ TILESONSCREEN + GetRandomValue(0, 5), 0 }, 42069.f / (rand() % 3600)));
      zombiesToPlace--;
    }
    else if (tileZombies)
    {
      if (spawnLocations[tileZombies-1].x != 0)
        zombies[i] = spawnLocations[tileZombies-1];
      tileZombies--;
    }
  


  // Get the chunk that the player is in
  int px = player.pos.x > 0 ? (int) player.pos.x : (int) player.pos.x - 1;
  int py = player.pos.y > 0 ? (int) player.pos.y : (int) player.pos.y - 1;
  py++;
  if (px < 10) px++;
  int chunkx = px > 0 ? (int)(px-1) / 128 : (int) px / 128 - 1;
  int chunky = py > 0 ? (int)(py-1) / 128 : (int) py-- / 128 - 1;
  int offsetx = (unsigned int)(px - 1) %  128;
  int offsety = (unsigned int)(py - 1) %  128;
  offsetx = (int)(player.pos.x - CHUNKSIZE * chunkx);
  offsety = (int)(player.pos.y - CHUNKSIZE * chunky);


  // Make sure that we know where the active chunks around us are
  // int x, y;
  for (int i = 0; i < 4; ++i)
  {
    if ((int) activeChunks[i].pos.x != chunkx)
      activeChunkExistsX = -1 * (chunkx - (int) activeChunks[i].pos.x);
    if ((int) activeChunks[i].pos.y != chunky)
      activeChunkExistsY = -1 * (chunky - (int) activeChunks[i].pos.y);
  }

  // Perform check to see if player can move to tile (Check collision)
  player.pos = Vector2Add(player.pos, scheduledMovement);
  int canMoveX = 1, canMoveY = 1;
  if (*getTile(Vector2Add(player.pos, (Vector2){ 0, -0.29 })))
    canMoveY = false;
  else
  {
    if (*getTile(Vector2Add((Vector2){ player.pos.x - scheduledMovement.x , player.pos.y }, (Vector2){ -0.29, -0.29 })))
      canMoveY = 0;
    if (*getTile(Vector2Add((Vector2){ player.pos.x - scheduledMovement.x , player.pos.y }, (Vector2){ -0.29, 0.29 })))
      canMoveY = 0;
  }
  if (*getTile(Vector2Add(player.pos, (Vector2){ 0, 0.29 })))
    canMoveY = false;
  else
  {
    if (*getTile(Vector2Add((Vector2){ player.pos.x - scheduledMovement.x , player.pos.y }, (Vector2){ 0.29, -0.29 })))
      canMoveY = 0;
    if (*getTile(Vector2Add((Vector2){ player.pos.x - scheduledMovement.x , player.pos.y }, (Vector2){ 0.29, 0.29 })))
      canMoveY = 0;
  }
  if (*getTile(Vector2Add(player.pos, (Vector2){ -0.29, 0 })))
    canMoveX = false;
  else
  {
    if (*getTile(Vector2Add((Vector2){ player.pos.x, player.pos.y - scheduledMovement.y }, (Vector2){ -0.29, -0.29 })))
      canMoveX = 0;
    if (*getTile(Vector2Add((Vector2){ player.pos.x, player.pos.y - scheduledMovement.y }, (Vector2){ 0.29, -0.29 })))
      canMoveX = 0;
  }
  if (*getTile(Vector2Add(player.pos, (Vector2){ 0.29, 0 })))
    canMoveX = false;
  else
  {
    if (*getTile(Vector2Add((Vector2){ player.pos.x, player.pos.y - scheduledMovement.y }, (Vector2){ -0.29, 0.29 })))
      canMoveX = 0;
    if (*getTile(Vector2Add((Vector2){ player.pos.x, player.pos.y - scheduledMovement.y }, (Vector2){ 0.29, 0.29 })))
      canMoveX = 0;
  }
  // printf("%d, %d\n", canMoveX, canMoveY);

  /* Legacy useless shit code that was create at 3 am
  Vector2 checkDirections[9] = {
    // (Vector2){ 0, 0 },
    (Vector2){ -0.31, -0.31 },
    // (Vector2){ 0, -0.31 },
    (Vector2){ 0.31, -0.31 },
    // (Vector2){ 0.31, 0 },
    (Vector2){ 0.31, 0.31 },
    // (Vector2){ 0, 0.31 },
    (Vector2){ -0.31, 0.31 },
    // (Vector2){ -0.31, 0 },
  };
  for (int dir = 0; dir < 4; ++dir)
    if (*getTile(Vector2Add(checkDirections[dir], player.pos)) == 1)
    {
      Vector2 pTileMid = Vector2Add(checkDirections[dir], (Vector2){ px + 0.5f, py + 0.5f });
      Vector2 diff = Vector2Subtract(player.pos, pTileMid);
      if (diff.x < 0.5 || diff.x > -0.5)
      {
        // player.pos.x -= scheduledMovement.x;
        canMoveX = false;
        printf("%f %f\n", diff.x, diff.y);
      }
      if (diff.y < 0.5 || diff.y > -0.5)
      {
        // player.pos.y -= scheduledMovement.y;
        canMoveY = false;
      }
    }
  */
  if (!canMoveX) player.pos.x -= scheduledMovement.x;
  if (!canMoveY) player.pos.y -= scheduledMovement.y;

  // Load more chunks in the correct direction
  // Check if the player is near the edge of a chunk and 'shift' the active chunks
  int slot1, slot2;
  // Check if we need to load chunks to the right
  if (offsetx > CHUNKSIZE - WIDECHUNKS / 2 && activeChunkExistsX == -1)
  {
    printf("activeChunkExists: %d %d\n", activeChunkExistsX, activeChunkExistsY);
    printf("Loading chunks to the right, offsetx: %d\n", offsetx);
    // Save chunks at the left (unactivate them)
    slot1 = findChunk(4, activeChunks, chunkx - 1, chunky, 0);
    slot2 = findChunk(4, activeChunks, chunkx - 1, chunky + activeChunkExistsY, 0);
    printf("slots %d %d\n", slot1, slot2);
    saveActiveChunk(slot1);
    saveActiveChunk(slot2);
    // Load chunks on the right
    loadChunk(slot1, chunkx + 1, chunky);
    loadChunk(slot2, chunkx + 1, chunky + activeChunkExistsY);
    activeChunkExistsX = 1;
  }
  // Check if we need to load chunks to the left
  else if (offsetx < WIDECHUNKS / 2 && activeChunkExistsX == 1)
  {
    printf("activeChunkExists: %d %d\n", activeChunkExistsX, activeChunkExistsY);
    printf("Loading chunks to the left, offsetx: %d\n", offsetx);
    // Save chunks at the right (unactivate them)
    slot1 = findChunk(4, activeChunks, chunkx + 1, chunky, 0);
    slot2 = findChunk(4, activeChunks, chunkx + 1, chunky + activeChunkExistsY, 0);
    printf("slots %d %d\n", slot1, slot2);
    saveActiveChunk(slot1);
    saveActiveChunk(slot2);
    // Load chunks on the left
    loadChunk(slot1, chunkx - 1, chunky);
    loadChunk(slot2, chunkx - 1, chunky + activeChunkExistsY);
    activeChunkExistsX = -1;
  }
  // Check if we need to load chunks at the bottom
  if (offsety > CHUNKSIZE - WIDECHUNKS / 2 && activeChunkExistsY == -1)
  {
    printf("activeChunkExists: %d %d\n", activeChunkExistsX, activeChunkExistsY);
    printf("Loading chunks to the bottom, offsetx: %d\n", offsety);
    // Save chunks at the top (unactivate them)
    slot1 = findChunk(4, activeChunks, chunkx, chunky - 1, 0);
    slot2 = findChunk(4, activeChunks, chunkx + activeChunkExistsX, chunky - 1, 0);
    printf("slots %d %d\n", slot1, slot2);
    saveActiveChunk(slot1);
    saveActiveChunk(slot2);
    // Load chunks on the bottom
    loadChunk(slot1, chunkx, chunky + 1);
    loadChunk(slot2, chunkx + activeChunkExistsX, chunky + 1);
    activeChunkExistsY = 1;
  }
  // Check if we need to load chunks at the top
  else if (offsety < WIDECHUNKS / 2 && activeChunkExistsY == 1)
  {
    printf("activeChunkExists: %d %d\n", activeChunkExistsX, activeChunkExistsY);
    printf("Loading chunks to the top, offsetx: %d\n", offsety);
    // Save chunks at the bottom (unactivate them)
    slot1 = findChunk(4, activeChunks, chunkx, chunky + 1, 0);
    slot2 = findChunk(4, activeChunks, chunkx + activeChunkExistsX, chunky + 1, 0);
    printf("slots %d %d\n", slot1, slot2);
    saveActiveChunk(slot1);
    saveActiveChunk(slot2);
    // Load chunks on the top
    loadChunk(slot1, chunkx, chunky - 1);
    loadChunk(slot2, chunkx + activeChunkExistsX, chunky - 1);
    activeChunkExistsY = -1;
  }

  return 0;
}


int drawGame()
{
  // Check the resolution of the window in case it has been resized
  if (IsWindowFullscreen())
  {
    int display = GetCurrentMonitor();
    sW = GetMonitorWidth(display);
    sH = GetMonitorHeight(display);
  } else {
    sW = GetScreenWidth();
    sH = GetScreenHeight();
  }
  mainCam.offset.x = sW / 2.f;
  mainCam.offset.y = sH / 2.f;
  // Calculate size of tiles
  int tileSize = sH / (float) TILESONSCREEN;
  // Overdraw tiles to prevent gaps
  float otileSize = tileSize * 1.1f;

  #ifdef debug
  for (int c = 0; c < 4; ++c)
    DrawRectangleLines(activeChunks[c].pos.x * tileSize * CHUNKSIZE, activeChunks[c].pos.y * tileSize * CHUNKSIZE, tileSize * CHUNKSIZE, tileSize * CHUNKSIZE, RED);
  #endif /* ifdef debug */
  // Draw active chunks
  Rectangle tileRect;
  for (int c = 0; c < 4; ++c)
    for (int x = 0; x < CHUNKSIZE; ++x)
      for (int y = 0; y < CHUNKSIZE; ++y)
      {
        Vector2 realPos = {
          x + CHUNKSIZE * activeChunks[c].pos.x,
          y + CHUNKSIZE * activeChunks[c].pos.y,
        };
        Vector2 screenPos = {
          realPos.x + -1.f * player.pos.x + 0.0000001f,
          realPos.y + -1.f * player.pos.y + 0.0000001f,
        };
        int pixelPosX = screenPos.x * tileSize;
        int pixelPosY = screenPos.y * tileSize;
        if (pixelPosX > sW * 0.5 || pixelPosX < sW * -0.55)
          continue;
        if (pixelPosY > sH * 0.5 || pixelPosY < sH * -0.55)
          continue;
        Color col = { 0, 128, 45, 255};
        col.g = 128 + 4 * (randoms[x][y] % 3);
        col.g *= 1 - activeChunks[c].tiles[x][y];
        col.r = 200 * activeChunks[c].tiles[x][y];
        #ifdef debug
        if ((!x || x == 127) && (!y || y == 127)) col = RAYWHITE;
        #endif /* ifdef debug */
        DrawTextureEx(grassTex, (Vector2){ pixelPosX, pixelPosY }, 0.f, otileSize / 40.f, col);
        // DrawText(TextFormat("%d %d", (int) activeChunks[c].pos.x, (int) activeChunks[c].pos.y), screenPos.x * tileSize, screenPos.y * tileSize, tileSize / 5, RED);
      }

  // Draw gun range
  // Vector2 normalisedMouse;
  // if (!mouseMode)
  // {
  //   if (scheduledMovement.x != 0.f || scheduledMovement.y != 0.f)
  //     normalisedMouse = scheduledMovement;
  // }
  // else if (mouseMode == 1)
  // {
  //   if (scheduledMovement.x != 0.f || scheduledMovement.y != 0.f)
  //   normalisedMouse = Vector2Scale(scheduledMovement, -1.f);
  // }
  // else normalisedMouse = Vector2Add(GetMousePosition(), (Vector2){ -0.5 * sW, -0.5 * sH });

  float mouseAngle = Vector2Angle((Vector2){ 1, 1 }, normalisedMouse);
  Color col = { 245, 245, 245, 120 };
  // Flash the firing range yellow for 0.1s
  if (shotgunCooldown <= 0.1f)
  {
    col = YELLOW;
    col.a = 120;
  }
  DrawCircleSector((Vector2){ 0, 0 }, tileSize * 3, mouseAngle * 57.29573672, mouseAngle * 57.29573672 + 90, 30, col);
  // DrawCircleSector((Vector2){ 0, 0 }, tileSize, radianConvert(mouseAngle - 0.785398), radianConvert(mouseAngle + 0.785398), 10, (Color){ 245, 245, 245, 255 });
  //printf("%f %f\n", mouseAngle - 0.785398, mouseAngle + 0.785398);

  #ifdef debug
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) DrawRectangle(0 - sW / 2, 0, 10, 10, RED);
  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) DrawRectangle(0 - sW / 2, 10, 10, 10, RED);
  #endif /* ifdef debug */

  float ftileSize = sH / (float) TILESONSCREEN;
  // Draw zombies
  Texture2D zombieTex;
  for (int i = 0; i < MAXZOMBIES; ++i)
    if (zombies[i].x != 0)
    {
      if (zombies[i].x > player.pos.x)
        zombieTex = zombieRightWalk[((frameCount + i * 9) % (FPS / 4)) * 8 / FPS];
      else
        zombieTex = zombieLeftWalk[((frameCount + i * 9) % (FPS / 4)) * 8 / FPS];
      DrawTextureEx(zombieTex, Vector2Add(Vector2Scale(Vector2Subtract(zombies[i], player.pos), ftileSize), (Vector2){ ftileSize * -0.4, ftileSize * -0.5 }), 0.f, ftileSize / 8.0f, WHITE);
      #ifdef debug
      float angle = Vector2Angle(Vector2Subtract(normalisedMouse, player.pos), Vector2Subtract(zombies[i], player.pos));
      if (Vector2Distance(zombies[i], player.pos) <= 3)
      {
        // float angle = Vector2Angle(Vector2Subtract(normalisedMouse, player.pos), Vector2Subtract(zombies[i], player.pos));
        if (angle > -0.785398 && angle < 0.785398) DrawCircleV(Vector2Scale(Vector2Subtract(zombies[i], player.pos), tileSize), tileSize * 0.3, RED);
        else DrawCircleV(Vector2Scale(Vector2Subtract(zombies[i], player.pos), tileSize), tileSize * 0.3, PURPLE);
      }
      Vector2 tpos = Vector2Scale(Vector2Subtract(zombies[i], player.pos), tileSize);
      DrawText(TextFormat("%f", zombies[i].x), tpos.x, tpos.y, 20, RED);
      #endif /* ifdef debug */
    }

  // Draw player
  // DrawRectangle(ftileSize * -0.3, ftileSize * -0.3, ftileSize * 0.6, ftileSize * 0.6, BLUE);
  // DrawRectangleLines(ftileSize * -0.3, ftileSize * -0.3, ftileSize * 0.6, ftileSize * 0.6, BLACK);
  #ifdef debug
  // printf("%f %f %f\n", mouseAngle, GetMousePosition().x, GetMousePosition().y);
  #endif /* ifdef debug */
  
  // Anime player
  // DrawTextureRec(manLeft, (Rectangle){ 0, 0, ftileSize * 06, ftileSize * 06 }, (Vector2){ ftileSize * -03, ftileSize * -03}, (Color){ 128, 128, 128, 255 });
  Texture2D manTex = facing?manLeft:manRight;
  if (scheduledMovement.x != 0.f || scheduledMovement.y != 0.f)
  {
    if (facing)
      manTex = manLeftWalk[((frameCount + 69) % (FPS / 5)) * 10 / FPS];
    else
      manTex = manRightWalk[((frameCount + 69) % (FPS / 5)) * 10 / FPS];
  }
  DrawTextureEx(manTex, (Vector2){ ftileSize * -0.5, ftileSize * -0.5}, 0.f, ftileSize / 8.f, WHITE);

  // Subtract 45 degrees
  Vector2 v1 = Vector2Rotate((Vector2){ tileSize * 1.f, tileSize * 0.4 }, mouseAngle + 0.785398);
  Vector2 v2 = Vector2Rotate((Vector2){ tileSize * 1.f, tileSize * -0.4 }, mouseAngle + 0.785398);
  Vector2 v3 = Vector2Rotate((Vector2){ tileSize * 1.4, 0 }, mouseAngle + 0.785398);
  DrawTriangle(v1, v3, v2, (Color){ 00, 00, 255, 100 });
  // DrawTriangleLines(v1, v2, v3, BLACK);
  return 0;
}


int drawUI()
{
  #ifdef debug
  int px = player.pos.x > 0 ? (int) player.pos.x : (int) player.pos.x - 1;
  int py = player.pos.y > 0 ? (int) player.pos.y : (int) player.pos.y - 1;
  py++;
  if (px < 10) px++;
  DrawText(TextFormat("Pos: %d, %d | Raw Pos: %f %f", px, py, player.pos.x, player.pos.y), 10, 10, 20, RED);
  int pCx = px > 0 ? (int) px / 128 : (int) px / 128 - 1;
  int pCy = py > 0 ? (int) py / 128 : (int) py-- / 128 - 1;
  DrawText(TextFormat("Chunk: %d, %d", pCx, pCy), 10, 40, 20, RED);
  DrawText(TextFormat("Chunk offset: %d, %d", (unsigned int)(px-1) % 128, (unsigned int)(py-1) % 128), 10, 70, 20, RED);
  DrawText(TextFormat("aCE: %d, %d", activeChunkExistsX, activeChunkExistsY), 10, 100, 20, RED);
  #endif /* ifdef debug */
  // Pause border to easily see that game is paused
  if (gamePaused) DrawRectangleLinesEx((Rectangle){ 0, 0, sW, sH }, 20, (Color){ 230, 41, 55, 128 });
  // Draw Score
  int tileSize = sH / TILESONSCREEN;
  const char *scorestring = TextFormat("Score: %d", player.kills);
  DrawText(scorestring, (sW - MeasureText(scorestring, tileSize)) / 2, 10, tileSize, RED);
  // Draw mouseMode
  const char *mouseModeText = "Mouse";
  if (!mouseMode)
    mouseModeText = "Keyboard";
  else if (mouseMode == 1)
    mouseModeText = "Inverted Keyboard";
  DrawText(mouseModeText, sW - MeasureText(mouseModeText, tileSize) - tileSize/2, sH - tileSize*3/2, tileSize, RED);
  // Paused Or Game Over
  if (gamePaused && !playerDead) drawScreen(PAUSED);
  else if (gamePaused && playerDead) drawScreen(GAMEOVER);
  // Draw FPS
  DrawFPS(10, sH - 30);
  return 0;
}


int drawScreen(int screen)
{ 
  int width, height;
  const int tileSize = sH / TILESONSCREEN;
  switch (screen) {
  case PLAYING:
    // Do nothing
    break;

  case PAUSED:
    width = MeasureText("GAME PAUSED", tileSize * 3);
    height = tileSize * 3;
    DrawText("GAME PAUSED", (sW-width)/2, (sH-height)/2, tileSize * 3, RED);
    break;

  case START:
    ClearBackground((Color){ 20, 20, 20, 255 });
    break;

  case GAMEOVER:
    ClearBackground((Color){ 220, 20, 20, 255 });
    DrawText("You Died", (sW - MeasureText("You Died", tileSize * 2)) / 2, sH / 2 - 5 - tileSize * 2, tileSize * 2, (Color){ 255, 20, 120, 255 });
    const char *scorestring = TextFormat("Score: %d", player.kills);
    DrawText(scorestring, (sW - MeasureText(scorestring, tileSize * 2)) / 2, sH / 2 + 5, tileSize * 2, (Color){ 255, 20, 120, 255 });
    DrawText(
        "Press Enter to return to Start",
        (sW - MeasureText("Press Enter to return to Start", tileSize / 2)) / 2,
        sH / 2 + 5 + tileSize * 2,
        tileSize / 2,
        (Color){ 255, 20, 120, 255 }
    );
    break;

  default:
    // Do nothing
    break;

} return 0; }


int xorShift32(int state)
{
  int x = state;
  for (int i = 0; i < 83; ++i)
  {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    x++;
  }
  return x;
}

int findChunk(int length, struct mapChunk searchlist[length], int xPos, int yPos, int flags)
{
  for (int i = 0; i < length; ++i)
    if ((int) searchlist[i].pos.x == xPos && (int) searchlist[i].pos.y == yPos)
      if (!(flags & 1) || openSlots[i] != -1)
        return i;
  return -1;
}

// Save a chunk in the active chunk list to the chunk cache
int saveActiveChunk(int slot)
{
  // Iterate over chunk list and replace the oldest chunk
  int oldest = 0;
  for (int i = 0; i < MAXCHUNKS; ++i)
  {
    if (openSlots[i] > openSlots[oldest])
      oldest = i;
    if (openSlots[i] != -1)
      openSlots[i]++;
  }
  // Copy over the oldest chunk
  openSlots[oldest] = 0;
  for (int x = 0; x < CHUNKSIZE; ++x)
    for (int y = 0; y < CHUNKSIZE; ++y)
      chunks[oldest].tiles[x][y] = activeChunks[slot].tiles[x][y];
  chunks[oldest].pos = activeChunks[slot].pos;
  
  printf("Chunk saved to index %d\n", oldest);
  return 0;
}

// Load a chunk from chunk cache, if it does not exist, create an empty chunk
int loadChunk(int slot, int xPos, int yPos)
{
  int chunk = findChunk(MAXCHUNKS, chunks, xPos, yPos, 1);
  if (chunk == -1)
  {
    printf("Chunk NOT found: %d, %d\n", xPos, yPos);
    memset(&activeChunks[slot], 0, sizeof(activeChunks[slot]));
    activeChunks[slot].pos = (Vector2){ xPos, yPos };
  }
  else
  {
    printf("Chunk found: %d, %d\n", xPos, yPos);
    for (int x = 0; x < CHUNKSIZE; ++x)
      for (int y = 0; y < CHUNKSIZE; ++y)
         activeChunks[slot].tiles[x][y] = chunks[chunk].tiles[x][y];
    activeChunks[slot].pos = chunks[chunk].pos;
    openSlots[chunk] = -1;
  }
  return 0;
}

// Wrap a radian around
float radianConvert(float angle)
{
  if (angle < 0)
    return 6.28319 + angle;
  else if (angle > 6.28319)
    return angle - 6.28319;
  else
    return angle;
}

char *getTile(Vector2 pos)
{
  int px = pos.x > 0 ? (int) pos.x : (int) pos.x - 1;
  int py = pos.y > 0 ? (int) pos.y : (int) pos.y - 1;
  py++;
  if (px < 10) px++;
  int pCx = px > 0 ? (int) px / 128 : (int) px / 128 - 1;
  int pCy = py > 0 ? (int) py / 128 : (int) py-- / 128 - 1;
  int chunkTileX = (int)(pos.x - CHUNKSIZE * pCx);
  int chunkTileY = (int)(pos.y - CHUNKSIZE * pCy);
  return &activeChunks[findChunk(4, activeChunks, pCx, pCy, 0)].tiles[chunkTileX][chunkTileY];
}

/* Not implemented (TODO)
int spiralFindTile(Vector2 pos, int *x, int *y, int *activeChunk, char match)
{

  return 0;
}
*/

int startScreen()
{
  frameCount++;
  // Animeate player being chased by zombie

  // Check the resolution of the window in case it has been resized
  if (IsWindowFullscreen())
  {
    int display = GetCurrentMonitor();
    sW = GetMonitorWidth(display);
    sH = GetMonitorHeight(display);
  } else {
    sW = GetScreenWidth();
    sH = GetScreenHeight();
  }
  // ---
  const char *controls[5] = { NULL };
  controls[0] = "k - Toggle between keyboard aiming modes";
  controls[1] = "m - Enable mouse aiming";
  controls[2] = "p - pause / start game";
  controls[3] = "space - fire";
  controls[4] = "esc - quit";
  Texture2D zombieTex = zombieLeftWalk[(frameCount % (FPS / 4)) * 8 / FPS];
  Texture2D playerTex = manLeftWalk[((frameCount + 69) % (FPS / 5)) * 10 / FPS];
  float tileSize = sH / (float) TILESONSCREEN;
  BeginDrawing();
  ClearBackground((Color){ 0, 132, 45, 255 });
  DrawText("Hoard Avoidance", (sW - MeasureText("Hoard Avoidance", tileSize * 4)) / 2, tileSize, tileSize * 4, GREEN);
  DrawTextureEx(zombieTex, (Vector2){ sW * 0.35, sH * 0.325 }, 0.f, tileSize / 4.f, WHITE);
  DrawTextureEx(zombieTex, (Vector2){ sW * 0.3, sH * 0.275 }, 0.f, tileSize / 4.f, WHITE);
  DrawTextureEx(playerTex, (Vector2){ sW * 0.6, sH * 0.3 }, 0.f, tileSize / 4.f, WHITE);
  for (int i = 0; i < 5; ++i)
    DrawText(controls[i], (sW - MeasureText(controls[i], tileSize / 2)) / 2, sH / 2.f + i * tileSize, tileSize / 2, GREEN);
  const char *mouseModeText = "Mouse";
  if (!mouseMode)
    mouseModeText = "Keyboard";
  else if (mouseMode == 1)
    mouseModeText = "Inverted Keyboard";
  DrawText(mouseModeText, sW - MeasureText(mouseModeText, tileSize) - tileSize/2, sH - tileSize*3/2, tileSize, RED);
  EndDrawing();
  return 0;
}
