#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <string.h>

#define CHUNKSIZE 128
#define cameraZoom 1.0f
#define MAXCHUNKS 25
#define TILESONSCREEN 20
#define WIDECHUNKS 50 
#define FPS 120
#define SPEED 30
#define debug true

enum {UP, DOWN, LEFT, RIGHT}; // Directions
enum {PLAYING, GAMEOVER, START}; // Game screens

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

static int gamePaused = true;
static int sW = 1280;
static int sH = 720;

static Vector2 zombies[1000] = { 0 };

static struct player player = { 0 };
static Camera2D mainCam = { 0 };

int fullscreenAdjust();
int xorShift32(int state);
int findChunk(int length, struct mapChunk chunks[length], int xPos, int yPos, int flags);

int setupGame();
int handleControls();
int tick();
int drawGame();
int drawUI();

int main(int argc, char *argv[])
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);    // Window configuration flags
  InitWindow(1280, 720, "Hoard avoidance");
  SetTargetFPS(FPS);
  #ifndef debug
  fullscreenAdjust();
  #endif /* ifndef debug */

  // Pregenerate the random textures so that rendering is faster
  for (int x = 0; x < CHUNKSIZE; ++x)
    for (int y = 0; y < CHUNKSIZE; ++y)
      randoms[x][y] = xorShift32(xorShift32((int)(x) ^ 1455093647) ^ xorShift32((int)(y) ^ 1455093647));

  // Set up game variables
  setupGame();

  // Main loop
  while (!WindowShouldClose())
  {
    // Take keyboard inputs
    handleControls();
    // Update game variables
    tick();

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

  return 0;
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

  if (IsKeyDown(KEY_W))
    player.pos.y -= (float) SPEED / FPS;
  if (IsKeyDown(KEY_S))
    player.pos.y += (float) SPEED / FPS;
  if (IsKeyDown(KEY_A))
    player.pos.x -= (float) SPEED / FPS;
  if (IsKeyDown(KEY_D))
    player.pos.x += (float) SPEED / FPS;

  #ifdef debug
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
  {
    int tileSize = sH / (float) TILESONSCREEN;
    int px = player.pos.x > 0 ? (int) player.pos.x : (int) player.pos.x - 1;
    int py = player.pos.y > 0 ? (int) player.pos.y : (int) player.pos.y - 1;
    int chunkx = px > 0 ? (int) px / 128 : (int) px / 128 - 1;
    int chunky = py > 0 ? (int) py / 128 : (int) py / 128 - 1;
    int chunk = findChunk(4, activeChunks, chunkx, chunky, 0);
    Vector2 normalisedMouse = Vector2Add(GetMousePosition(), (Vector2){ -0.5 * sW, -0.5 * sH });
    int mouseTileX = (int)(normalisedMouse.x / tileSize + px) % 128;
    int mouseTileY = (int)(normalisedMouse.y / tileSize + py) % 128;
    activeChunks[chunk].tiles[mouseTileX][mouseTileY] = 1;
  }
  #endif /* ifdef debug */
  return 0;
}


int tick()
{
  // Check if the player is near the edge of a chunk and shift the active chunks
  // Get the chunk that the player is in
  int px = player.pos.x > 0 ? (int) player.pos.x : (int) player.pos.x - 1;
  int py = player.pos.y > 0 ? (int) player.pos.y : (int) player.pos.y - 1;
  int chunkx = px > 0 ? (int)(px-1) / 128 : (int) px / 128 - 1;
  int chunky = py > 0 ? (int)(py-1) / 128 : (int) py / 128 - 1;
  int offsetx = (unsigned int)(px - 1) %  128;
  int offsety = (unsigned int)(py - 1) %  128;
  // Load more chunks in the correct direction

  // Make sure that we know where the active chunks around us are
  int x, y;
  for (int i = 0; i < 4; ++i)
  {
    if ((int) activeChunks[i].pos.x != chunkx)
      activeChunkExistsX = -1 * (chunkx - (int) activeChunks[i].pos.x);
    if ((int) activeChunks[i].pos.y != chunky)
      activeChunkExistsY = -1 * (chunky - (int) activeChunks[i].pos.y);
  }

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
  // Draw active chunks
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
        col.r = 255 * activeChunks[c].tiles[x][y];
        DrawRectangle(pixelPosX, pixelPosY, otileSize, otileSize, col);
        // DrawText(TextFormat("%d %d", (int) activeChunks[c].pos.x, (int) activeChunks[c].pos.y), screenPos.x * tileSize, screenPos.y * tileSize, tileSize / 5, RED);
      }
  // Draw player
  DrawRectangle(tileSize * -0.3, tileSize * -0.3, otileSize * 0.6, otileSize * 0.6, BLUE);
  DrawRectangleLines(tileSize * -0.3, tileSize * -0.3, otileSize * 0.6, otileSize * 0.6, BLACK);
  float mouseAngle = Vector2Angle((Vector2){ 1, 1 }, Vector2Add(GetMousePosition(), (Vector2){ -0.5 * sW, -0.5 * sH }));
  #ifdef debug
  // Vector2 normalisedMouse = Vector2Add(GetMousePosition(), (Vector2){ -0.5 * sW, -0.5 * sH });
  // printf("%f %f %f\n", mouseAngle, GetMousePosition().x, GetMousePosition().y);
  #endif /* ifdef debug */
  // Subtract 45 degrees
  Vector2 v1 = Vector2Rotate((Vector2){ tileSize * 0.5, tileSize * 0.4 }, mouseAngle + 0.785398);
  Vector2 v2 = Vector2Rotate((Vector2){ tileSize * 0.5, tileSize * -0.4 }, mouseAngle + 0.785398);
  Vector2 v3 = Vector2Rotate((Vector2){ tileSize * 0.9, 0 }, mouseAngle + 0.785398);
  DrawTriangle(v1, v3, v2, BLUE);
  DrawTriangleLines(v1, v2, v3, BLACK);

  return 0;
}


int drawUI()
{
  #ifdef debug
  int px = player.pos.x > 0 ? (int) player.pos.x : (int) player.pos.x - 1;
  int py = player.pos.y > 0 ? (int) player.pos.y : (int) player.pos.y - 1;
  DrawText(TextFormat("Pos: %d, %d", px, py), 10, 10, 20, RED);
  int pCx = px > 0 ? (int) px / 128 : (int) px / 128 - 1;
  int pCy = py > 0 ? (int) py / 128 : (int) py / 128 - 1;
  DrawText(TextFormat("Chunk: %d, %d", pCx, pCy), 10, 40, 20, RED);
  DrawText(TextFormat("Chunk offset: %d, %d", (unsigned int)(px-1) % 128, (unsigned int)(py-1) % 128), 10, 70, 20, RED);
  DrawText(TextFormat("aCE: %d, %d", activeChunkExistsX, activeChunkExistsY), 10, 100, 20, RED);
  #endif /* ifdef debug */
  // Draw FPS
  DrawFPS(10, sH - 30);
  return 0;
}


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
