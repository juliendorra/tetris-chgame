#include <CHGame.h>
#include "View.h"
#include "src/DmgAssets.h"

namespace view {
namespace {
constexpr uint16_t DIM=0x100;
constexpr uint8_t EMPTY=0x2f, POINTER=0xf0, HEART=0xf1;
unsigned paletteChoice=0;
uint16_t previousMap[360];
// Keep the working map off the 2 KB stack, leaving room for audio interrupts
// during the area-filtering pass. This adds 720 bytes, not a pixel buffer.
uint16_t drawingMap[360];
bool previousValid=false,previousTitle=false;
// Four DMG luminance families, with extra entries for filtered pixel coverage.
const uint8_t COLORS[3][4][3]={
    {{202,220,159},{147,176,104},{73,112,65},{22,49,36}},
    {{155,188,15},{139,172,15},{48,98,48},{15,56,15}},
    {{239,242,221},{178,187,162},{98,110,88},{25,37,29}},
};
uint8_t letter(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='z')c+='A'-'a';
    if(c>='A' && c<='Z')return c-'A'+10;
    if(c=='-')return 0x25;
    return EMPTY;
}
void text(uint16_t *map,int x,int y,const char *s,bool dim=false) {
    for(;*s && x<20;++s,++x)
        if(x>=0 && y>=0 && y<18)map[y*20+x]=letter(*s)|(dim?DIM:0);
}
void number(uint16_t *map,int right,int y,unsigned value,unsigned digits=0) {
    // The board's small printf does not implement dynamic field widths.
    char s[12];s[11]=0;char *p=s+11;
    do { *--p='0'+value%10;value/=10; } while(value && p>s);
    while(unsigned(s+11-p)<digits && p>s)*--p='0';
    text(map,right-int(s+11-p),y,p);
}
void base(uint16_t *map,const uint8_t *source) {
    for(unsigned i=0;i<360;++i)map[i]=source[i];
}
void rect(uint16_t *map,int x,int y,int w,int h,uint8_t tile=EMPTY) {
    for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)
        if(row>=0 && row<18 && col>=0 && col<20)map[row*20+col]=tile;
}
void table(uint16_t *map,const Record *records) {
    for(unsigned i=0;i<3;++i)if(records[i].score) {
        text(map,4,13+i,records[i].name);
        number(map,18,13+i,records[i].score,6);
    }
}
void difficulty(uint16_t *map,const Scene &s,const Record *records) {
    base(map,s.type?dmg::LEVEL_B_MAP:dmg::LEVEL_A_MAP);
    table(map,records);
    const bool blink=(s.frame/16)%2==0;
    if(s.screen!=Level || blink) {
        const int x=(s.type?2:5)+(s.level%5)*2;
        map[(6+s.level/5*2)*20+x]=s.level;
    }
    if(s.type && (s.screen!=Height || blink))
        map[(6+s.height/3*2)*20+13+s.height%3*2]=s.height;
    if(s.heart)map[1*20+8]=HEART;
}
uint8_t block(uint8_t value) {
    static const uint8_t tile[]={EMPTY,0x84,0x81,0x80,0x83,0x82,0x86,0x85};
    if((value&tetris::KIND_MASK)!=tetris::I+1)return tile[value&tetris::KIND_MASK];
    static const uint8_t segments[]={0x80,0x80,0x88,0x89,0x8a,0x8b,0x8f};
    return segments[value>>3];
}
void piece(uint16_t *map,unsigned kind,unsigned rotation,int x,int y) {
    const uint16_t shape=tetris::shape(kind,rotation);
    static const uint8_t tiles[]={0x84,0x81,0x8b,0x83,0x82,0x86,0x85};
    for(int cy=0;cy<4;++cy)for(int cx=0;cx<4;++cx) {
        if(!tetris::cell(shape,cx,cy))continue;
        const int px=x+cx,py=y+cy;
        if(px<0 || px>=20 || py<0 || py>=18)continue;
        unsigned tile=tiles[kind];
        if(kind==tetris::I) {
            if(rotation&1)tile=cy==0?0x80:cy==3?0x89:0x88;
            else tile=cx==0?0x8a:cx==3?0x8f:0x8b;
        }
        map[py*20+px]=tile;
    }
}
void field(uint16_t *map,const tetris::Game &g,const Scene &s) {
    base(map,g.type?dmg::PLAY_B_MAP:dmg::PLAY_A_MAP);
    if(!g.type) {
        rect(map,13,3,6,1);number(map,19,3,g.score);
        number(map,18,7,g.level);number(map,19,10,g.lines);
    } else {
        number(map,18,2,g.level);number(map,17,5,g.height);
        number(map,18,10,g.remaining(),2);
    }
    if(g.heart)map[(g.type?2:7)*20+14]=HEART;
    if(s.preview)piece(map,g.next,0,15,12);
    if(s.screen==Pause) {
        const char *lines[]={"  HIT   ","        "," START  ","        ","   TO   ",
                             "        ","CONTINUE","        ","  GAME  ","        "};
        for(int i=0;i<10;++i) {
            text(map,3,4+i,lines[i]);
            if(i%2)for(int c=0;c<8;++c)
                if(lines[i-1][c]!=' ')map[(4+i)*20+3+c]=0xad;
        }
        return;
    }
    for(int y=0;y<18;++y) {
        const int source=g.rowSource(y);
        for(int x=0;x<10;++x) {
            if(source==tetris::FLASH_ROW)map[y*20+2+x]=0x8c;
            else if(source>=0 && g.board[source][x])
                map[y*20+2+x]=block(g.board[source][x]);
        }
    }
    if(g.phase==tetris::Falling)piece(map,g.piece,g.rotation,2+g.x,g.y);
}
void result(uint16_t *map,const tetris::Game &g,const Scene &s) {
    field(map,g,s);
    if(g.phase==tetris::Lost) {
        static const uint8_t panel[7][8]={
            {0x61,0x62,0x62,0x62,0x62,0x62,0x62,0x63},
            {0x64,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,0x65},
            {0x64,EMPTY,16,10,22,14,EMPTY,0x65},
            {0x64,EMPTY,0xad,0xad,0xad,0xad,EMPTY,0x65},
            {0x64,EMPTY,24,31,14,27,EMPTY,0x65},
            {0x64,EMPTY,0xad,0xad,0xad,0xad,EMPTY,0x65},
            {0x66,0x69,0x69,0x69,0x69,0x69,0x69,0x6a},
        };
        for(int y=0;y<7;++y)for(int x=0;x<8;++x)map[(y+5)*20+3+x]=panel[y][x];
    } else {
        rect(map,2,0,10,18);
        const char *names[]={"SINGLE","DOUBLE","TRIPLE","TETRIS"};
        const char *multipliers[]={"40","100","300","1200"};
        const unsigned values[]={40,100,300,1200};
        for(int i=0;i<4;++i) {
            text(map,2,i*3,names[i]);
            number(map,4,i*3+1,g.clears[i]);map[(i*3+1)*20+5]=0x26;
            text(map,7,i*3+1,multipliers[i]);
            number(map,11,i*3+2,g.clears[i]*values[i]*(g.level+1));
        }
        unsigned linePoints=0;
        for(unsigned i=0;i<4;++i)linePoints+=g.clears[i]*values[i]*(g.level+1);
        text(map,2,12,"DROPS");number(map,11,13,g.score>=linePoints?g.score-linePoints:0);
        for(int x=2;x<12;++x)map[15*20+x]=0xad;
        text(map,2,16,"THIS STAGE");number(map,11,17,g.score);
    }
}
// Decode scanlines rather than allocating a second full framebuffer in SRAM.
static RAMFUNC(dmgSourceRow) void sourceRow(const uint16_t *map,bool title,int y,uint8_t *out,unsigned from,unsigned to) {
    if(y>=144) { memset(out,3,160);return; }
    static const uint8_t arrow[]={0x20,0x30,0x38,0x3c,0x38,0x30,0x20,0};
    static const uint8_t heart[]={0,0x66,0xff,0xff,0x7e,0x3c,0x18,0};
    for(unsigned col=from;col<to;++col) {
        const unsigned entry=map[(y/8)*20+col],index=entry&255;
        if(index==POINTER || index==HEART) {
            unsigned bits=(index==POINTER?arrow:heart)[y&7];
            for(unsigned x=0;x<8;++x)out[col*8+x]=bits&(128>>x)?3:0;
            continue;
        }
        const uint8_t *tile=index<39?dmg::FONT[index]:
            title?dmg::TITLE[index-39]:index<48?dmg::TITLE[index-39]:dmg::GAME[index-48];
        const uint16_t pixels=tile[(y&7)*2]|(uint16_t(tile[(y&7)*2+1])<<8);
        for(unsigned x=0;x<8;++x) {
            unsigned value=(pixels>>(x*2))&3;
            if(entry&DIM)value=value?0:1;
            out[col*8+x]=value;
        }
    }
}
// An exact 5/4 footprint turns four source greys into 16 coverage shades.
static RAMFUNC(dmgPresent) void present(const uint16_t *map,bool title) {
    uint8_t left[116],right[116];
#ifdef CHSIM_FORCE_FULL
    const bool full=true;
#else
    const bool full=!previousValid || previousTitle!=title;
#endif
    memset(left,full?0:128,sizeof left);memset(right,full?128:0,sizeof right);
    if(!previousValid)gfx_clear(15);
    if(!full)for(unsigned i=0;i<360;++i)if(map[i]!=previousMap[i]) {
        const unsigned x=i%20,y=i/20;
        const unsigned x0=(x*32/5)&~1u,x1=(((x+1)*32+4)/5+1)&~1u;
        const unsigned y0=y*32/5,y1=((y+1)*32+4)/5;
        for(unsigned row=y0;row<y1;++row) {
            if(left[row]>x0)left[row]=x0;
            if(right[row]<x1)right[row]=x1;
        }
    }
    uint8_t a[160],b[160];
    for(unsigned y=0;y<116;++y) {
        if(left[y]>=right[y])continue;
        const unsigned sy=y*5/4,wy=4-(y*5%4);
        const unsigned from=left[y]*5/32,to=((right[y]-1)*5/4+1)/8+1;
        sourceRow(map,title,sy,a,from,to);sourceRow(map,title,sy+1,b,from,to);
        uint8_t *dst=gfx_fb+(y+6)*64;
        for(unsigned x=left[y];x<right[y];x+=2) {
            unsigned packed=0;
            for(unsigned sub=0;sub<2;++sub) {
                const unsigned sx=(x+sub)*5/4,wx=4-((x+sub)*5%4);
                const unsigned top=a[sx]*wx+a[sx+1]*(5-wx);
                const unsigned bottom=b[sx]*wx+b[sx+1]*(5-wx);
                const unsigned coverage=(top*wy+bottom*(5-wy)+2)/5;
                packed|=coverage<<(sub*4);
            }
            dst[x/2]=packed;
        }
    }
    memcpy(previousMap,map,sizeof previousMap);previousValid=true;previousTitle=title;
}
}
void palette(unsigned choice) {
    paletteChoice=choice%3;
    uint16_t ramp[16];
    for(unsigned i=0;i<16;++i) {
        unsigned shade=i/5,fraction=i%5;
        if(paletteChoice==1) { shade=(i+2)/5;fraction=0; }
        const unsigned next=shade<3?shade+1:3;
        uint8_t rgb[3];
        for(unsigned c=0;c<3;++c)
            rgb[c]=(COLORS[paletteChoice][shade][c]*(5-fraction)+COLORS[paletteChoice][next][c]*fraction+2)/5;
        ramp[i]=gfx_rgb(rgb[0],rgb[1],rgb[2]);
    }
    gfx_setPalette(ramp,16);
}
void draw(const tetris::Game &g,const Scene &s,const Record *records) {
    uint16_t *map=drawingMap;
    const bool blink=(s.frame/16)%2==0;
    if(s.screen==Title) {
        base(map,dmg::TITLE_MAP);text(map,12,14,"OPTIONS");
        if(blink)map[14*20+(s.titleChoice?11:1)]=POINTER;
    } else if(s.screen==GameType || s.screen==Music) {
        base(map,dmg::MENU_MAP);
        text(map,3,12,"A-TYPE",s.music!=0 || (s.screen==Music && !blink));
        text(map,11,12,"B-TYPE",s.music!=1 || (s.screen==Music && !blink));
        text(map,3,14,"C-TYPE",s.music!=2 || (s.screen==Music && !blink));
        text(map,12,14,"OFF",s.music!=3 || (s.screen==Music && !blink));
        if(s.screen!=GameType || blink)text(map,s.type?11:3,5,s.type?"B-TYPE":"A-TYPE");
    } else if(s.screen==Play || s.screen==Pause)field(map,g,s);
    else if(s.screen==Level || s.screen==Height || s.screen==Records || s.screen==Name) {
        difficulty(map,s,records);
        if(s.screen==Name) {
            text(map,4,13+s.rank,s.name);
            if(!blink)map[(13+s.rank)*20+4+s.namePosition]=EMPTY;
        }
    } else if(s.screen==Result)result(map,g,s);
    else {
        base(map,dmg::MENU_MAP);rect(map,2,2,16,14);
        text(map,6,2,"CONTROLS");
        text(map,3,4,"LEFT RIGHT MOVE");text(map,3,6,"A ROTATE LEFT");
        text(map,3,7,"B ROTATE RIGHT");text(map,3,9,"DOWN SOFT DROP");
        text(map,3,10,"START PAUSE");text(map,3,11,"SELECT NEXT");
        text(map,3,13,"TITLE SELECT");text(map,3,14,"CHANGES PALETTE");
        text(map,5,16,"A B BACK");
    }
    present(map,s.screen==Title);
}
}
