#include <raylib.h>
#include <stdio.h>

#define chunkSize 128
#define cameraZoom 1.0f;
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
  char tiles[chunkSize][chunkSize];
  Vector2 *zombies; // Free list and set to NULL when chunk not in range
};

static int gamePaused = true;
static int sW = 1280;
static int sH = 720;

static struct player player = { 0 };
static Camera2D mainCam = { 0 };

int fullscreenAdjust();

int setupGame();
int handleKeyBoard();
int tick();
int drawGame();
int drawUI();

int main(int argc, char *argv[])
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);    // Window configuration flags
  InitWindow(1280, 720, "Hoard avoidance");
  fullscreenAdjust();


  // Set up game variables
  setupGame();

  // Main loop
  while (!WindowShouldClose())
  {
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
  // Setup player
  player.pos.x = 0.f;
  player.pos.y = 0.f;
  player.weapon = 1;
  player.kills = 0;
  player.money = 50;

  mainCam.target = (Vector2){ 0.f, 0.f };
  mainCam.zoom = 1.f;
  mainCam.offset = (Vector2){ 0.f, 0.f };

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

  return 0;
}


int drawUI()
{
  return 0;
}

