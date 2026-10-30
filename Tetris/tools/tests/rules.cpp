#include "../../Game.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace tetris;

static void empty(Game &g) {
    g.begin(0,0,0,false,12345);
    memset(g.board,0,sizeof g.board);
    g.phase=Falling;g.events=0;
}
static void well(Game &g,unsigned count,unsigned level=0) {
    empty(g);g.level=level;g.piece=I;g.rotation=1;g.x=3;g.y=14;
    for(unsigned y=HEIGHT-count;y<HEIGHT;++y)
        for(unsigned x=0;x<WIDTH;++x)g.board[y][x]=x==4?0:O+1;
    assert(g.fits(g.piece,g.rotation,g.x,g.y));
}
int main() {
    Game g;
    for(unsigned p=0;p<7;++p)for(unsigned r=0;r<4;++r) {
        assert(__builtin_popcount(shape(p,r))==4);
        empty(g);g.piece=p;g.rotation=r;g.x=3;g.y=5;
        assert(g.rotate(1));assert(g.rotate(-1));assert(g.rotation==r);
        while(g.move(-1)){}
        assert(!g.fits(p,r,g.x-1,g.y));
        while(g.move(1)){}
        assert(!g.fits(p,r,g.x+1,g.y));
        while(g.drop(false)){}
        unsigned count=0;for(auto &row:g.board)for(auto b:row)count+=bool(b);
        assert(count==4 && g.pieces==1);
    }
    // No wall kick: an I flush against the right wall cannot turn horizontal.
    empty(g);g.piece=I;g.rotation=1;g.x=8;g.y=4;
    assert(g.fits(I,1,8,4));assert(!g.rotate(1));assert(g.x==8 && g.rotation==1);
    // Clockwise and its inverse use the Nintendo pivot.
    empty(g);g.piece=J;g.y=4;assert(g.rotate(1));assert(g.rotation==3);
    assert(shape(J,g.rotation)==0x2230);
    // Gravity and the original 23/9 frame horizontal repeat.
    empty(g);g.piece=O;int y=g.y;
    for(unsigned f=0;f<52;++f)g.frame(0);
    assert(g.y==y);g.frame(0);assert(g.y==y+1);
    // Actual DMG drop intervals include the zero-counter frame. Measure every
    // level and heart mode through the engine, rather than only its table.
    const unsigned intervals[]={53,49,45,41,37,33,28,22,17,11,10,9,8,7,6,6,5,5,4,4,3};
    for(unsigned heart=0;heart<2;++heart)for(unsigned level=0;level<=20;++level) {
        empty(g);g.level=level;g.heart=heart;g.piece=O;g.y=0;g.dropTimer=g.gravity();
        const unsigned speed=level+(heart?10:0),interval=intervals[speed>20?20:speed];
        for(unsigned drop=0;drop<4;++drop) {
            const int before=g.y;
            for(unsigned f=1;f<interval;++f) {g.frame(0);assert(g.y==before);}
            g.frame(0);assert(g.y==before+1);
        }
    }
    empty(g);g.piece=O;int x=g.x;g.frame(LEFT);assert(g.x==x-1);
    for(unsigned f=0;f<22;++f)g.frame(LEFT);
    assert(g.x==x-1);g.frame(LEFT);assert(g.x==x-2);
    for(unsigned f=0;f<9;++f)g.frame(LEFT);assert(g.x==x-3);
    // Releasing DOWN cancels the soft-drop scoring streak.
    empty(g);g.piece=O;g.frame(DOWN);assert(g.softPoints==1);
    g.frame(0);assert(g.softPoints==0);
    while(g.phase==Falling)g.frame(DOWN);
    assert(g.score>0);assert(!g.downArmed);
    for(unsigned f=0;f<5;++f)g.frame(DOWN);
    assert(g.phase==Falling && g.y==-2 && g.softPoints==0);
    g.frame(0);g.frame(DOWN);assert(g.y==-1);
    // All four clear sizes, score multiplier, animation and stable compaction.
    const unsigned values[]={40,100,300,1200};
    for(unsigned n=1;n<=4;++n)for(unsigned level: {0u,4u,9u}) {
        well(g,n,level);g.board[10][0]=S+1;
        g.drop(false);assert(g.phase==Clearing && g.lastClear==n);
        assert(g.score==values[n-1]*(level+1));assert(g.lines==0);
        for(unsigned f=0;f<CLEAR_FRAMES-1;++f)g.frame(0);
        assert(g.lines==0);g.frame(0);assert(g.lines==n);
        assert(g.board[10+n][0]==S+1 && g.clears[n-1]==1);
        unsigned filled=0;for(auto &row:g.board)for(auto b:row)filled+=bool(b);
        assert(filled==5-n);
        if(n<4)assert(g.board[17][4]==lockedBlock(I,1,1,3-n));
    }
    // Locked I cells retain orientation/end caps even next to other I pieces.
    for(unsigned rotation=0;rotation<4;++rotation) {
        empty(g);g.piece=I;g.rotation=rotation;g.x=3;g.y=10;
        while(g.drop(false)){}
        const unsigned mask=shape(I,rotation);
        for(int cy=0;cy<4;++cy)for(int cx=0;cx<4;++cx)if(cell(mask,cx,cy))
            assert(g.board[g.y+cy][g.x+cx]==lockedBlock(I,rotation,cx,cy));
    }
    well(g,1);g.lines=9;g.drop(false);
    for(unsigned f=0;f<CLEAR_FRAMES;++f)g.frame(0);
    assert(g.level==1 && g.gravity()==48);
    well(g,4,7);g.lines=21;g.type=1;g.drop(false);
    for(unsigned f=0;f<CLEAR_FRAMES;++f)g.frame(0);
    assert(g.phase==Won && g.remaining()==0 && g.level==7);
    // B-Type heights leave the upper field clear and never generate full rows.
    for(unsigned h=0;h<6;++h)for(unsigned seed=1;seed<100;++seed) {
        g.begin(1,9,h,false,seed);assert(g.phase==Falling && g.remaining()==25);
        for(int row=0;row<HEIGHT;++row) {
            unsigned n=0;for(auto b:g.board[row])n+=bool(b);
            if(row<HEIGHT-int(h*2))assert(!n);else assert(n<10);
        }
        assert(g.gravity()==10);
    }
    g.begin(0,9,0,true,1);assert(g.gravity()==3);
    empty(g);memset(g.board,1,sizeof g.board);g.spawn();assert(g.phase==Lost);
    g.score=999990;g.addScore(12000);assert(g.score==999999);
    // Replay is deterministic and random sequences allow droughts/repeats.
    Game a,b;a.begin(0,0,0,false,42);b=a;
    bool repeat=false;unsigned last=255;
    for(unsigned i=0;i<2000;++i) {
        unsigned p=a.choose(),q=b.choose();assert(p==q && p<7);
        repeat|=last==p;last=p;a.next=b.next=p;
    }
    assert(repeat);
    // Long adversarial play traverses input, collisions, top-outs and clears
    // under UBSan; every live piece must always occupy valid cells.
    uint32_t rng=123;unsigned games=0;
    empty(g);
    for(unsigned f=0;f<500000;++f) {
        rng=rng*1664525+1013904223;
        uint8_t keys=DOWN;
        if(rng&16)keys|=rng&32?LEFT:RIGHT;
        if((rng&15)==0)keys|=CW;
        if((rng&31)==1)keys|=CCW;
        g.frame(keys);
        if(g.phase==Lost || g.phase==Won) {++games;g.begin(0,rng%10,0,false,rng);}
        if(g.phase==Falling)assert(g.fits(g.piece,g.rotation,g.x,g.y));
        for(auto &row:g.board)for(auto v:row)
            assert(v<=7 || ((v&KIND_MASK)==I+1 && v>>3>=1 && v>>3<=6));
        assert(g.score<=999999 && g.level<=20);
    }
    assert(games>10);
    printf("PASS: rotations, walls, gravity, DAS, drop streaks, four clear sizes, scoring, levels, B-Type, seeds; 500000 frames / %u games\n",games);
}
