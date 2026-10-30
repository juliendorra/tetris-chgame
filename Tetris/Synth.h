#pragma once
#include <stdint.h>
namespace chip {
enum Effect { Move, Turn, Land, Clear, Tetris, LevelUp, Victory, GameOver, Menu };
void effect(Effect effect);
void setTempo(unsigned percent); // 100 normal, 125 danger; does not restart.
constexpr unsigned SAMPLE_RATE = 31250;
struct Voice { uint16_t increment; uint8_t volume, note, duty, wave; };
void begin();
void play(unsigned track);
void pause(bool paused);
bool paused();
void setVolume(unsigned value); // 0..4
unsigned volume();
void mute(unsigned channel, bool value);
unsigned muteMask();
void millisecond();
uint8_t sample(); // Same mixer used by the hardware ISR and WAV renderer.
Voice voice(unsigned channel);
uint32_t ticks();
void snapshot(int8_t *out); // 128-sample oscilloscope
}
