#pragma once
#include <stdint.h>

namespace tetrisaudio {
// A conservative, mostly linear output stage for the original DMG voices.
// Pulse instruments get their separate balance adjustment in Synth.cpp.
// Keeping the knee near the PWM limit avoids compressing normal note decays.
class Output {
    int32_t dcQ16 = 0;
public:
    __attribute__((always_inline)) inline uint8_t silence() {
        dcQ16 = 0;
        return 128;
    }
    __attribute__((always_inline)) inline uint8_t write(int sample, unsigned volume) {
        if (!volume) return silence();
        if (volume > 4) volume = 4;
        const int32_t target = sample * 65536;
        dcQ16 += (target - dcQ16) >> 9;
        int value = (target - dcQ16) * 3 / 131072; // 1.5x, approximately 10 Hz DC removal.
        if (value > 112) value = 112 + (value - 112) / 4;
        else if (value < -112) value = -112 + (value + 112) / 4;
        if (value > 120) value = 120;
        else if (value < -120) value = -120;
        return uint8_t(128 + value * int(volume) / 4);
    }
};
}
