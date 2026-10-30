#pragma once
#include <stdint.h>

namespace tetris {
constexpr int WIDTH = 10, HEIGHT = 18;
// DMG: two lock frames, three grey/block flashes (10 frames per half),
// thirteen blank frames, then one field row per frame, bottom to top.
constexpr unsigned CLEAR_FLASH_START = 2, CLEAR_FLASH_STEP = 10;
constexpr unsigned CLEAR_BLANK_START = 62, CLEAR_SWEEP_START = 75;
constexpr unsigned CLEAR_FRAMES = CLEAR_SWEEP_START + HEIGHT - 1;
constexpr int EMPTY_ROW = -1, FLASH_ROW = -2;
enum Piece : uint8_t { J, L, I, O, Z, S, T };
constexpr uint8_t KIND_MASK = 7;
// I segments retain their original tile identity, including end caps, in the
// high bits of the existing board byte. They never join a neighboring piece.
uint8_t lockedBlock(uint8_t kind, uint8_t rotation, int cx, int cy);
// CHGame's face buttons are A on the left, B on the right, opposite the DMG.
enum Button : uint8_t { CCW=1, CW=2, DOWN=8, LEFT=16, RIGHT=32 };
enum Phase : uint8_t { Falling, Clearing, Entering, Lost, Won };
enum Event : uint16_t { Moved=1, Rotated=2, Locked=4, Line=8, Four=16, Level=32, Win=64, Lose=128 };
uint16_t shape(uint8_t piece, uint8_t rotation);
bool cell(uint16_t mask, int x, int y);

// Rules are independent of drawing, sound, Arduino and wall-clock time.
struct Game {
    uint8_t board[HEIGHT][WIDTH] = {};
    uint32_t random = 1, score = 0, clearRows = 0;
    uint16_t lines = 0, pieces = 0, events = 0;
    uint16_t clears[4] = {};
    uint8_t type = 0, startLevel = 0, level = 0, height = 0;
    uint8_t piece = T, next = I, future = O, rotation = 0;
    int8_t x = 3, y = -2;
    Phase phase = Falling;
    uint8_t timer = 0, dropTimer = 0, shiftTimer = 0, softTimer = 0;
    uint8_t held = 0, softPoints = 0, lastClear = 0;
    bool heart = false, downArmed = true;

    void begin(uint8_t gameType, uint8_t initialLevel, uint8_t initialHeight,
               bool fast, uint32_t seed);
    void frame(uint8_t buttons);
    bool fits(uint8_t p, uint8_t r, int px, int py) const;
    bool move(int dx);
    bool rotate(int direction);
    bool drop(bool soft);
    void lock();
    void collapse();
    // Displayed source row during the clear; negative values select blank/grey.
    int rowSource(int row) const;
    void spawn();
    uint8_t gravity() const;
    uint8_t choose();
    uint32_t roll();
    uint16_t remaining() const { return lines >= 25 ? 0 : 25 - lines; }
    bool danger() const;
    void addScore(uint32_t points);
};
}
