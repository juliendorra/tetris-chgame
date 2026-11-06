#include "Synth.h"
#include "src/Songs.h"
#include "Output.h"
#include <string.h>
#if defined(__riscv) && !defined(CHSIM)
#include <Arduino.h>
#include <chgame/RamFunc.h>
#define CHIP_DEVICE 1
#else
#define CHIP_DEVICE 0
#define RAMFUNC(name)
#endif

namespace {
// Interpret only the original music data; the PWM output and cue mixer are
// retained from the ATM demo. No Game Boy CPU or executable ROM is needed.
struct Lane {
    uint16_t list=0, at=0, increment=0;
    uint8_t remaining=1, duration=0, note=0, age=0;
    uint8_t envelope=0, level=0, envelopeTimer=0, duty=0, wave=3, trigger=0;
};
Lane lanes[4];
tetrisaudio::Output output;
volatile bool ready=false, isPaused=false;
volatile unsigned gain=4, muted=0, tempo=100;
uint32_t tickFraction=0, tempoFraction=0;
volatile uint32_t tickCounter=0;
unsigned envelopeFraction=0;
volatile uint16_t effectMs=0, effectDuration=0;
volatile unsigned effectId=0;
uint16_t effectPhase=0, phase[3]={}, noisePhase=0, noise=0x7fff;
uint8_t seen[4]={};
volatile int8_t scope[128];
unsigned scopeIndex=0,scopeDivider=0;
struct Mix { uint16_t inc[3]; uint8_t vol[4], duty[2], wave, trigger[4]; };
Mix controls[2]={};
volatile unsigned front=0;
uint8_t scoreByte(uint16_t address) { return DMG_MUSIC[address-DMG_MUSIC_BASE]; }
uint16_t scoreWord(uint16_t address) { return scoreByte(address)|(uint16_t(scoreByte(address+1))<<8); }
uint16_t increment(unsigned note,int vibrato,bool wave) {
    if(!note)return 0;
    const unsigned period=DMG_PITCHES[note]+vibrato;
    // DMG pulse: clock/(32*(2048-period)); wave: clock/(64*(2048-period)).
    return uint64_t(DMG_CLOCK)*2048/((2048-period)*chip::SAMPLE_RATE)/(wave?2:1);
}
void publish() {
    const unsigned next=front^1;
    for(unsigned ch=0;ch<4;++ch) {
        const Lane &s=lanes[ch];
        unsigned vol=s.note ? s.level*3 : 0;
        if(ch==2 && s.note) {
            static const unsigned levels[]={0,24,12,6};
            vol=levels[(s.duty>>5)&3];
            if((s.duty&128) && s.remaining<=6)vol/=2;
        }
        controls[next].vol[ch]=(isPaused || (muted&(1u<<ch)))?0:vol;
        if(ch<3)controls[next].inc[ch]=s.increment;
        if(ch<2)controls[next].duty[ch]=s.duty>>6;
        if(ch==2)controls[next].wave=s.wave;
        controls[next].trigger[ch]=s.trigger;
    }
    asm volatile("" ::: "memory");front=next;
}
void step() {
    for(unsigned ch=0;ch<4;++ch) {
        Lane &s=lanes[ch];
        if(!s.list)continue;
        if(--s.remaining) {
            ++s.age;
            if(ch<3 && s.note && (s.duty&15)) {
                const unsigned index=s.age/2;
                const unsigned packed=DMG_VIBRATO[index<55?index:54];
                const unsigned nibble=s.age&1 ? packed&15 : packed>>4;
                s.increment=increment(s.note,nibble&8?int(nibble)-16:int(nibble),ch==2);
            }
            continue;
        }
        while(true) {
            const unsigned command=scoreByte(s.at++);
            if(command==0) {
                s.list+=2;
                if(scoreWord(s.list)==0xffff)s.list=scoreWord(s.list+2);
                s.at=scoreWord(s.list);continue;
            }
            if(command==0x9d) {
                const unsigned lo=scoreByte(s.at++),hi=scoreByte(s.at++);
                s.duty=scoreByte(s.at++);
                if(ch==2)s.wave=lo==0xa9?0:lo==0xb9?1:lo==0xc9?2:3;
                else s.envelope=hi?0:lo; // Mirrors the source driver's unused high-byte path.
                continue;
            }
            if((command&0xf0)==0xa0) {s.duration=DMG_LENGTHS[command&15];continue;}
            s.remaining=s.duration;s.age=0;s.envelopeTimer=0;
            s.note=command==1?0:(ch==3?(command-1)/5:command/2);
            if(ch==3)s.envelope=DMG_NOISE[command];
            s.level=s.envelope>>4;
            if(ch<3)s.increment=increment(s.note,0,ch==2);
            if(s.note)++s.trigger;
            break;
        }
    }
    publish();
}
}

namespace chip {
void play(unsigned track) {
    if(track>=SONG_COUNT)return;
    ready=false;
#if CHIP_DEVICE
    __disable_irq();
#endif
    for(unsigned ch=0;ch<4;++ch) {
        lanes[ch]=Lane{};lanes[ch].list=SONGS[track].channels[ch];
        if(lanes[ch].list)lanes[ch].at=scoreWord(lanes[ch].list);
    }
    output.silence();
    memset(phase,0,sizeof phase);memset(seen,0,sizeof seen);noise=0x7fff;noisePhase=0;
    tickFraction=tempoFraction=tickCounter=0;envelopeFraction=0;effectMs=0;isPaused=false;
    step();ready=true;
#if CHIP_DEVICE
    __enable_irq();
#endif
}
void pause(bool p) {
#if CHIP_DEVICE
    __disable_irq();
#endif
    isPaused=p;if(p)effectMs=0;publish();
#if CHIP_DEVICE
    __enable_irq();
#endif
}
bool paused() {return isPaused;}
void setVolume(unsigned v) {gain=v>4?4:v;}
unsigned volume() {return gain;}
void mute(unsigned ch,bool value) {
    if(ch>=4)return;
#if CHIP_DEVICE
    __disable_irq();
#endif
    if(value)muted|=1u<<ch;else muted&=~(1u<<ch);publish();
#if CHIP_DEVICE
    __enable_irq();
#endif
}
unsigned muteMask() {return muted;}
void effect(Effect e) {
    static const uint16_t durations[]={18,30,65,200,400,280,1500,900,25};
    effectId=unsigned(e);effectDuration=durations[unsigned(e)];effectMs=effectDuration;
}
void setTempo(unsigned percent) {tempo=percent>200?200:percent;}
void millisecond() {
    if(effectMs)--effectMs;
    if(!ready || isPaused)return;
    // Score durations are original DMG video frames, at 4194304/70224 Hz.
    // Hardware envelopes run separately at 64 Hz, retaining note articulations.
    envelopeFraction+=64;
    if(envelopeFraction>=1000) {
        envelopeFraction-=1000;
        for(unsigned ch=0;ch<4;++ch)if(ch!=2) {
            Lane &s=lanes[ch];const unsigned period=s.envelope&7;
            if(period && ++s.envelopeTimer>=period) {
                s.envelopeTimer=0;
                if(s.envelope&8) {if(s.level<15)++s.level;}
                else if(s.level)--s.level;
            }
        }
        publish();
    }
    tempoFraction+=DMG_CLOCK*tempo;
    tickFraction+=tempoFraction/100;tempoFraction%=100;
    if(tickFraction>=DMG_FRAME_CYCLES*1000) {
        tickFraction-=DMG_FRAME_CYCLES*1000;step();++tickCounter;
    }
}
RAMFUNC(chipSample) uint8_t sample() {
    if(!ready)return output.silence();
    const Mix &m=controls[front];int mix=0;
    static const uint8_t widths[]={32,64,128,192};
    for(unsigned ch=0;ch<3;++ch) {
        if(ch==2 && seen[ch]!=m.trigger[ch])phase[ch]=0;
        seen[ch]=m.trigger[ch];phase[ch]+=m.inc[ch];
        // Static output balance only: retain every original envelope step,
        // pulse duty, pitch and wave sample. Both pulse parts receive the
        // same lift, so tunes that exchange melody and harmony still work.
        const int v=ch<2 ? int(m.vol[ch])*3/2 : m.vol[ch];
        if(ch<2)mix+=(phase[ch]>>8)<widths[m.duty[ch]]?v:-v;
        else {
            const unsigned at=phase[ch]>>11,packed=DMG_WAVES[m.wave][at/2];
            const int wave=(at&1?packed&15:packed>>4)*2-15;
            mix+=wave*v/15;
        }
    }
    if(seen[3]!=m.trigger[3]) {noise=0x7fff;noisePhase=0;seen[3]=m.trigger[3];}
    const unsigned nextNoise=unsigned(noisePhase)+34359; // Original $3A: 16384 Hz, 7-bit LFSR.
    noisePhase=nextNoise;
    if(nextNoise>=65536) {
        const unsigned feedback=(noise^(noise>>1))&1;
        noise=(noise>>1)|(feedback<<14);noise=(noise&~(1u<<6))|(feedback<<6);
    }
    // Balance the music percussion against the pulse melody on CHGame.
    const int drumLevel=m.vol[3]/2;
    mix+=noise&1?-drumLevel:drumLevel;
    if (effectMs) {
        const unsigned elapsed = effectDuration - effectMs;
        unsigned hz = 900;
        switch (effectId) {
        case Move: hz = 1100; break;
        case Turn: hz = 1600; break;
        case Land: hz = 180 + effectMs * 4; break;
        case Clear: hz = 700 + elapsed * 5; break;
        case Tetris: hz = 1000 + (elapsed / 60 % 4) * 330; break;
        case LevelUp: hz = 880 + (elapsed / 70) * 220; break;
        case Victory: {
            static const unsigned notes[] = {523,659,784,1047,784,1047};
            hz = notes[(elapsed / 180) % 6]; break;
        }
        case GameOver: hz = 600 - elapsed / 2; break;
        case Menu: hz = 1300; break;
        }
        // Routine controls should not pump the entire soundtrack. Keep
        // their clicks short and low; only major events gently duck music.
        effectPhase += uint16_t(hz * 65536ul / SAMPLE_RATE);
        const unsigned ceiling=effectId==Move ? 3 : effectId==Turn ? 4 :
                               effectId==Menu ? 6 : effectId==Land ? 8 : 12;
        const int v = effectMs < ceiling ? effectMs : ceiling;
        if (effectId>=Clear && effectId<=GameOver) mix=mix*7/8;
        mix += effectPhase & 0x8000 ? v : -v;
    }
    if (++scopeDivider == 8) {
        scopeDivider = 0;
        scope[scopeIndex++ & 127] = int8_t(mix * int(gain) / 4);
    }
    if (!(m.vol[0] | m.vol[1] | m.vol[2] | m.vol[3]) && !effectMs) return output.silence();
    return output.write(mix, gain);
}
Voice voice(unsigned ch) {
    if(ch>=4)return {};
    return {ch<3?lanes[ch].increment:uint16_t(0),controls[front].vol[ch],lanes[ch].note,lanes[ch].duty,lanes[ch].wave};
}
uint32_t ticks() {return tickCounter;}
void snapshot(int8_t *out) {
    const unsigned start=scopeIndex;
    for(unsigned i=0;i<128;++i)out[i]=scope[(start+i)&127];
}
}

#if CHIP_DEVICE
extern "C" volatile uint32_t CFGHR_tmpB;
extern "C" void osSystickHandler(void) { chip::millisecond(); }
extern "C" void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" RAMFUNC(chipIRQ) void TIM3_IRQHandler(void) {
    TIM3->INTFR = ~1u;
    TIM1->CH2CVR = chip::sample();
}
#endif

void chip::begin() {
    ready = false;
#if CHIP_DEVICE
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1;
    RCC->APB1PCENR |= RCC_APB1Periph_TIM3;
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(7u << 15)) | (1u << 15);
    CFGHR_tmpB = (CFGHR_tmpB & ~(15u << 8)) | (11u << 8);
    GPIOB->CFGHR = CFGHR_tmpB;
    TIM1->CTLR1 = 0; TIM1->DMAINTENR = 0; TIM1->CCER = 0;
    TIM1->PSC = 0; TIM1->ATRLR = 255; TIM1->CNT = 0;
    TIM1->CHCTLR1 = 0x6800; TIM1->CH2CVR = 128;
    TIM1->RPTCR = 0; TIM1->BDTR = 0x8000; TIM1->CCER = 0x10;
    TIM1->SWEVGR = 1; TIM1->CTLR1 = 0x81; // 187.5 kHz PWM carrier
    TIM3->CTLR1 = 0; TIM3->DMAINTENR = 0;
    TIM3->PSC = 0; TIM3->ATRLR = 1535; TIM3->CNT = 0;
    TIM3->SWEVGR = 1; TIM3->INTFR = 0; TIM3->DMAINTENR = 1;
    NVIC_SetPriority(TIM3_IRQn, 0);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CTLR1 = 1; // 48 MHz / 1536 = exactly 31,250 samples/s
#endif
    play(0);
}
