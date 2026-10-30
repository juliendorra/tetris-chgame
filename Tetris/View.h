#pragma once
#include "Game.h"
namespace view {
enum Screen : uint8_t { Title, GameType, Music, Level, Height, Play, Pause, Result, Records, Help, Name };
struct Record { uint32_t score; char name[7]; uint8_t level, height; };
struct Scene {
    Screen screen = Title;
    uint8_t type = 0, music = 0, level = 0, height = 0, palette = 0;
    bool heart = false, preview = true;
    uint32_t frame = 0;
    uint8_t namePosition = 0, rank = 0, titleChoice = 0;
    char name[7] = "PLAYER";
};
void palette(unsigned choice);
void draw(const tetris::Game &game, const Scene &scene, const Record *records);
}
