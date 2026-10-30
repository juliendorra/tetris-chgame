#include "Game.h"
#include <string.h>

namespace tetris {
// Four-by-four Nintendo rotation boxes. Bit (y*4+x) is occupied.
// Orientations match the Game Boy's fixed pivots, including its two-state I/S/Z.
static constexpr uint16_t SHAPES[7][4] = {
    {0x1700,0x6220,0x0740,0x2230}, // J
    {0x4700,0x2260,0x0710,0x3220}, // L
    {0x0f00,0x2222,0x0f00,0x2222}, // I
    {0x6600,0x6600,0x6600,0x6600}, // O
    {0x6300,0x1320,0x6300,0x1320}, // Z
    {0x3600,0x2310,0x3600,0x2310}, // S
    {0x2700,0x2620,0x0720,0x2320}, // T
};
uint16_t shape(uint8_t p, uint8_t r) { return SHAPES[p % 7][r & 3]; }
bool cell(uint16_t m, int x, int y) { return (m & (uint16_t(1) << (y * 4 + x))) != 0; }
uint8_t lockedBlock(uint8_t kind, uint8_t rotation, int cx, int cy) {
    if (kind != I) return kind + 1;
    const unsigned segment = rotation & 1 ? (cy == 0 ? 1 : cy == 3 ? 3 : 2)
                                          : (cx == 0 ? 4 : cx == 3 ? 6 : 5);
    return (I + 1) | (segment << 3);
}
uint32_t Game::roll() {
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    return random;
}
uint8_t Game::choose() {
    // The original rejects (up to twice) using the bitwise OR of three
    // orientation-stripped piece IDs. A PRNG replaces the GB divider register.
    uint8_t candidate = 0;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        candidate = roll() % 7;
        if (attempt == 2 || (candidate | next | piece) != piece) break;
    }
    return candidate;
}
void Game::begin(uint8_t gameType, uint8_t initialLevel, uint8_t initialHeight,
                 bool fast, uint32_t seed) {
    *this = Game{};
    type = gameType ? 1 : 0;
    startLevel = level = initialLevel > 9 ? 9 : initialLevel;
    height = initialHeight > 5 ? 5 : initialHeight;
    heart = fast; random = seed ? seed : 1;
    if (type) {
        for (int row = HEIGHT - height * 2; row < HEIGHT; ++row) {
            const int hole = roll() % WIDTH;
            for (int col = 0; col < WIDTH; ++col)
                board[row][col] = col == hole || roll() % 2 == 0 ? 0 : 1 + roll() % 7;
        }
    }
    next = roll() % 7; future = roll() % 7;
    spawn();
}
uint8_t Game::gravity() const {
    static const uint8_t frames[] = {52,48,44,40,36,32,27,21,16,10,9,8,7,6,5,5,4,4,3,3,2};
    unsigned speed = level + (heart ? 10 : 0);
    return frames[speed > 20 ? 20 : speed];
}
bool Game::fits(uint8_t p, uint8_t r, int px, int py) const {
    const uint16_t mask = shape(p, r);
    for (int cy = 0; cy < 4; ++cy) for (int cx = 0; cx < 4; ++cx) {
        if (!cell(mask, cx, cy)) continue;
        const int bx = px + cx, by = py + cy;
        if (bx < 0 || bx >= WIDTH || by < 0 || by >= HEIGHT || board[by][bx]) return false;
    }
    return true;
}
bool Game::move(int dx) {
    if (!fits(piece, rotation, x + dx, y)) return false;
    x += dx; events |= Moved; return true;
}
bool Game::rotate(int direction) {
    const uint8_t r = (rotation + (direction > 0 ? 3 : 1)) & 3;
    if (!fits(piece, r, x, y)) return false;
    rotation = r; events |= Rotated; return true;
}
void Game::addScore(uint32_t points) {
    score += points;
    if (score > 999999) score = 999999;
}
bool Game::drop(bool soft) {
    if (fits(piece, rotation, x, y + 1)) {
        ++y;
        if (soft) ++softPoints;
        return true;
    }
    lock(); return false;
}
void Game::lock() {
    const uint16_t mask = shape(piece, rotation);
    for (int cy = 0; cy < 4; ++cy) for (int cx = 0; cx < 4; ++cx)
        if (cell(mask, cx, cy)) board[y + cy][x + cx] = lockedBlock(piece, rotation, cx, cy);
    addScore(softPoints); softPoints = 0; ++pieces;
    events |= Locked; clearRows = 0; lastClear = 0;
    for (int row = 0; row < HEIGHT; ++row) {
        bool full = true;
        for (int col = 0; col < WIDTH; ++col) full &= board[row][col] != 0;
        if (full) { clearRows |= 1ul << row; ++lastClear; }
    }
    downArmed = false;
    if (lastClear) {
        static const uint16_t points[] = {0,40,100,300,1200};
        addScore(uint32_t(points[lastClear]) * (level + 1));
        ++clears[lastClear - 1];
        events |= lastClear == 4 ? Four : Line;
        phase = Clearing; timer = CLEAR_FRAMES;
    } else { phase = Entering; timer = 2; }
}
void Game::collapse() {
    int dest = HEIGHT - 1;
    for (int row = HEIGHT - 1; row >= 0; --row)
        if (!(clearRows & (1ul << row))) {
            if (dest != row) memcpy(board[dest], board[row], WIDTH);
            --dest;
        }
    while (dest >= 0) memset(board[dest--], 0, WIDTH);
    lines += lastClear; clearRows = 0;
    if (type && lines >= 25) { phase = Won; events |= Win; return; }
    if (!type && level < 20 && lines >= (unsigned(level) + 1) * 10) {
        ++level; events |= Level;
    }
    spawn();
}
int Game::rowSource(int row) const {
    if (phase != Clearing) return row;
    const unsigned elapsed = CLEAR_FRAMES - timer;
    if (elapsed >= CLEAR_SWEEP_START && row >= HEIGHT - 1 - int(elapsed - CLEAR_SWEEP_START)) {
        // Keep the original board until the sweep completes. Its surviving rows
        // are read from bottom to top, so no extra field buffer is needed.
        int dest = HEIGHT - 1;
        for (int source = HEIGHT - 1; source >= 0; --source)
            if (!(clearRows & (1ul << source)) && dest-- == row) return source;
        return EMPTY_ROW;
    }
    if (!(clearRows & (1ul << row)) || elapsed < CLEAR_FLASH_START) return row;
    if (elapsed >= CLEAR_BLANK_START) return EMPTY_ROW;
    return ((elapsed - CLEAR_FLASH_START) / CLEAR_FLASH_STEP) & 1 ? row : FLASH_ROW;
}
void Game::spawn() {
    piece = next; next = future; future = choose();
    x = 3; y = -2; rotation = 0;
    dropTimer = gravity(); softTimer = 0; softPoints = 0;
    phase = Falling;
    if (!fits(piece, rotation, x, y)) { phase = Lost; events |= Lose; }
}
bool Game::danger() const {
    for (int row = 0; row < 6; ++row) for (int col = 0; col < WIDTH; ++col)
        if (board[row][col]) return true;
    return false;
}
void Game::frame(uint8_t buttons) {
    events = 0;
    const uint8_t pressed = buttons & ~held;
    held = buttons;
    if (!(buttons & DOWN)) { downArmed = true; softPoints = 0; softTimer = 0; }
    if (phase == Lost || phase == Won) return;
    if (phase == Clearing || phase == Entering) {
        if (timer && --timer) return;
        if (phase == Clearing) collapse(); else spawn();
        return;
    }
    if (pressed & CW) rotate(1);
    else if (pressed & CCW) rotate(-1);
    const uint8_t direction = buttons & RIGHT ? RIGHT : buttons & LEFT;
    if (direction) {
        bool shifting = pressed & direction;
        if (shifting) shiftTimer = 23;
        else if (shiftTimer && --shiftTimer == 0) { shifting = true; shiftTimer = 9; }
        if (shifting && !move(direction == RIGHT ? 1 : -1)) shiftTimer = 1;
    } else shiftTimer = 0;
    const bool soft = (buttons & (DOWN | LEFT | RIGHT)) == DOWN && downArmed;
    if (soft) {
        if (!softTimer || --softTimer == 0) { softTimer = 3; drop(true); }
    } else {
        softPoints = 0;
        if (dropTimer) --dropTimer;
        else { dropTimer = gravity(); drop(false); }
    }
}
}
