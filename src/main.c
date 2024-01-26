#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CHUNKSIZE 128
#define cameraZoom 1.0f
#define MAXCHUNKS 25
#define CHUNKSONSCREEN 20
#define FPS 120
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
  Vector2 *zombies; // Free list and set to NULL when chunk not in range
};

static struct mapChunk activeChunks[4];
static struct mapChunk chunks[MAXCHUNKS] = { 0 };

static int gamePaused = true;
static int sW = 1280;
static int sH = 720;

static struct player player = { 0 };
static Camera2D mainCam = { 0 };

int fullscreenAdjust();
int xorShift32(int state);

int setupGame();
int handleKeyBoard();
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

  // Set up game variables
  setupGame();

  // Main loop
  while (!WindowShouldClose())
  {
    // Take keyboard inputs
    handleKeyBoard();
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
  player.pos.x = 0.f;
  player.pos.y = 0.f;
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
  memset(activeChunks, 0, sizeof(activeChunks));
  activeChunks[0].pos = (Vector2){ -1.f, -1.f };
  activeChunks[1].pos = (Vector2){ 0.f, -1.f };
  activeChunks[2].pos = (Vector2){ -1.f, 0.f };
  activeChunks[3].pos = (Vector2){ 0.f, 0.f };

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


int handleKeyBoard()
{

  if (IsKeyDown(KEY_W))
    player.pos.y -= 5.f / FPS;
  if (IsKeyDown(KEY_S))
    player.pos.y += 5.f / FPS;
  if (IsKeyDown(KEY_A))
    player.pos.x -= 5.f / FPS;
  if (IsKeyDown(KEY_D))
    player.pos.x += 5.f / FPS;
  return 0;
}


int tick()
{
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
  // Render active chunks
  int tileSize = sH / (float) CHUNKSONSCREEN;
  // Overdraw tiles to prevent gaps
  float otileSize = tileSize * 1.1f;
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
        Color col = { 0, 128, 45, 255};
        int num = xorShift32((int)(realPos.x) ^ 1455093647) ^ xorShift32((int)(realPos.y) ^ 1455093647);
        col.g = 128 + 4 * (xorShift32(num) % 3);
        DrawRectangle(screenPos.x * tileSize, screenPos.y * tileSize, otileSize, otileSize, col);
      }

  return 0;
}


int drawUI()
{
  #ifdef debug
  DrawText(TextFormat("Pos: %d, %d", (int) player.pos.x, (int) player.pos.y), 10, 10, 20, RED);
  
  #endif /* ifdef debug
    DrawText(TextFormat("Pos: %d, %d", player.x, player.y), 10, 10, 20, RED); */
  return 0;
}


int xorShift32(int state)
{
  int x = state;
  for (int i = 0; i < 14; ++i)
  {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
  }
  return x;
}
