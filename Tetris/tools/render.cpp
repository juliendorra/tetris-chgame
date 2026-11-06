// Exercise and render the real tracker and mixer on a host, without Arduino.
#include "Synth.h"
#include "src/Songs.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>

static unsigned clockFraction = 0;
static void put16(FILE *f, unsigned n) { fputc(n & 255, f); fputc((n >> 8) & 255, f); }
static void put32(FILE *f, unsigned n) { put16(f, n); put16(f, n >> 16); }
static void wavHeader(FILE *f, unsigned samples) {
    fwrite("RIFF", 1, 4, f); put32(f, samples + 36); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, chip::SAMPLE_RATE);
    put32(f, chip::SAMPLE_RATE); put16(f, 1); put16(f, 8);
    fwrite("data", 1, 4, f); put32(f, samples);
}
static uint8_t nextSample() {
    clockFraction += 1000;
    if (clockFraction >= chip::SAMPLE_RATE) {
        clockFraction -= chip::SAMPLE_RATE;
        chip::millisecond();
    }
    return chip::sample();
}
int main(int argc, char **argv) {
    assert(argc == 2);
    chip::begin();
    const std::string out = argv[1];
    for (unsigned song = 0; song < SONG_COUNT; ++song) {
        const Song &s = SONGS[song];
        for (unsigned leadOnly = 0; leadOnly < 2; ++leadOnly) {
            chip::play(song);
            clockFraction = 0;
            for (unsigned ch = 0; ch < 4; ++ch) chip::mute(ch, leadOnly && ch != 1);
            const std::string name = out + "/" + std::to_string(song) + (leadOnly ? "-lead.wav" : "-full.wav");
            FILE *f = fopen(name.c_str(), "wb"); assert(f);
            unsigned count = uint64_t(s.length) * DMG_FRAME_CYCLES * chip::SAMPLE_RATE / DMG_CLOCK;
            wavHeader(f, count);
            for (unsigned i = 0; i < count; ++i) fputc(nextSample(), f);
            fclose(f);
        }
        // Dump actual tracker states through three loops for score verification.
        for (unsigned ch = 0; ch < 4; ++ch) chip::mute(ch, false);
        chip::play(song);
        FILE *trace = fopen((out + "/" + std::to_string(song) + ".tsv").c_str(), "w"); assert(trace);
        unsigned previous = ~0u;
        while (chip::ticks() < s.length * 3u) {
            if (chip::ticks() != previous) {
                previous = chip::ticks();
                fprintf(trace, "%u", previous);
                for (unsigned ch = 0; ch < 4; ++ch) {
                    auto v = chip::voice(ch);
                    fprintf(trace, "\t%u\t%u\t%u\t%u\t%u", v.note, v.volume, v.increment,v.duty,v.wave);
                }
                fputc('\n', trace);
            }
            chip::millisecond();
        }
        fclose(trace);
        unsigned tick = chip::ticks();
        chip::pause(true);
        for (unsigned i = 0; i < 5000; ++i) {
            chip::millisecond();
            assert(chip::sample() == 128);
        }
        assert(chip::ticks() == tick);
        chip::pause(false);
        for (unsigned i = 0; i < 1000; ++i) chip::millisecond();
        assert(chip::ticks() > tick);
        chip::setVolume(0);
        for (unsigned i = 0; i < 1000; ++i) assert(chip::sample() == 128);
        chip::setVolume(4);
    }
    // Danger speeds up the existing timeline; returning to normal retains it.
    chip::play(1);chip::setTempo(100);
    for(unsigned ms=0;ms<1000;++ms)chip::millisecond();
    const unsigned normal=chip::ticks();
    chip::setTempo(125);
    for(unsigned ms=0;ms<1000;++ms)chip::millisecond();
    const unsigned danger=chip::ticks()-normal;
    assert(danger>normal && danger<=normal*5/4+2);
    chip::setTempo(100);
    // Mix every cue over the loudest authored music, then verify that
    // envelopes terminate and pause cancels an in-progress effect.
    FILE *cues=fopen((out+"/cues.wav").c_str(),"wb");assert(cues);
    wavHeader(cues,chip::SAMPLE_RATE*18);
    for(unsigned e=0;e<9;++e) {
        chip::effect(chip::Effect(e));
        for(unsigned n=0;n<chip::SAMPLE_RATE*2;++n) {
            uint8_t pcm=nextSample();assert(pcm>=8 && pcm<=248);fputc(pcm,cues);
        }
    }
    fclose(cues);
    chip::effect(chip::Victory);chip::pause(true);
    for(unsigned n=0;n<1000;++n)assert(nextSample()==128);
    // Identical score starts with and without continuous routine inputs.
    // This catches soundtrack pumping that cannot be seen in note traces.
    for (unsigned controls=0;controls<2;++controls) {
        chip::play(0);chip::setVolume(4);clockFraction=0;
        FILE *f=fopen((out+(controls?"/routine-cues.wav":"/routine-clean.wav")).c_str(),"wb");assert(f);
        wavHeader(f,chip::SAMPLE_RATE*8);
        for(unsigned n=0;n<chip::SAMPLE_RATE*8;++n) {
            if(controls && n%(chip::SAMPLE_RATE/10)==0)
                chip::effect((n/(chip::SAMPLE_RATE/10))&1?chip::Turn:chip::Move);
            fputc(nextSample(),f);
        }
        fclose(f);
    }
    printf("PASS: %u tracks rendered; pause/resume, mute volume and long delays checked\n", SONG_COUNT);
}
