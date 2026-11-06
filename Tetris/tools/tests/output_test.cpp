#include "../../Output.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    // The useful range stays linear: no low-knee compression of note decays.
    tetrisaudio::Output output;
    double fundamental=0,harmonics=0;
    for (unsigned i=0;i<31250;++i) {
        const double phase=i*6.283185307179586*1000/31250;
        const int sample=int(60*std::sin(phase));
        const int value=int(output.write(sample,4))-128;
        if(i>=15625) {
            fundamental+=value*std::sin(phase);
            harmonics+=value*std::sin(phase*3);
        }
    }
    assert(std::abs(harmonics/fundamental)<0.01);
    const int offsets[]={-256,-80,80,256};
    for (int offset : offsets) {
        output.silence();
        for (unsigned i=0;i<20000;++i) {
            const int value=output.write(offset,4);
            assert(value>=8 && value<=248);
            if(i>16000)assert(std::abs(value-128)<=1);
        }
    }
    for(unsigned i=0;i<10000;++i) {
        const int input=int(i*131%513)-256;
        assert(output.write(input,0)==128);
        const int value=output.write(input,4);
        assert(value>=8 && value<=248);
    }
    puts("PASS: linear musical range, low harmonic distortion, DC rejection, exact mute and bounded PWM");
}
