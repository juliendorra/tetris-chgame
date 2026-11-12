// Game Boy-style single-player Tetris for CHGame Rev0.
#include <CHGame.h>
#include <CHSave.h>
#include "Game.h"
#include "View.h"
#include "Synth.h"

tetris::Game game;
view::Scene scene;
struct Saved {
    view::Record records[2][3] = {};
    uint8_t music = 0, palette = 0;
} saved;
static bool savePending = false, finished = false;
static view::Screen back = view::Title;
static uint16_t screenAge = 0;
static uint32_t seed = 0x54455452;
constexpr auto SAVE_ID = chsave::id("com.juliendorra.tetris");
static chsave::Store gameSaves;
static void loadSettings() {
    if(!gameSaves.load(SAVE_ID,2,saved)) {
        saved=Saved{};
        for(auto &group:saved.records)for(auto &r:group)memcpy(r.name,"------",7);
    }
    if(saved.music>3)saved.music=0;
    if(saved.palette>2)saved.palette=0;
    for(auto &group:saved.records)for(auto &r:group)r.name[6]=0;
}
#ifdef CHSIM
static uint32_t simulatedAt = 0;
#endif

static void screen(view::Screen next) { scene.screen = next; screenAge = 0; }
static void music(bool restart = true) {
    if (restart) chip::play(scene.music == 3 ? 0 : scene.music);
    chip::setVolume(4);
    chip::pause(scene.music == 3);
    chip::setTempo(100);
}
static void saveSettings() {
    saved.music = scene.music; saved.palette = scene.palette; savePending = true;
}
static void startGame() {
    game.begin(scene.type, scene.level, scene.height, scene.heart, seed ^ micros());
    scene.preview = true; finished = false; screen(view::Play);
    music(); saveSettings();
}
static void finishGame() {
    finished = true; screen(view::Result);
    chip::pause(true);
    chip::effect(game.phase == tetris::Won ? chip::Victory : chip::GameOver);
}
static void records() {
    view::Record *r = saved.records[scene.type];
    scene.rank = 3;
    for(unsigned i=0;i<3;++i) if(game.score > r[i].score) { scene.rank = i; break; }
    if(scene.rank<3) {
        for(unsigned i=2;i>scene.rank;--i)r[i]=r[i-1];
        r[scene.rank] = {};
        r[scene.rank].score = game.score; r[scene.rank].level = game.level; r[scene.rank].height = game.height;
        memcpy(scene.name,"PLAYER",7); scene.namePosition=0;
        screen(view::Name);
    } else { back=view::Level; screen(view::Records); music(); }
}
static void input() {
    const bool a=chgame.justPressed(A_BUTTON), b=chgame.justPressed(B_BUTTON);
    const bool start=chgame.justPressed(START_BUTTON), select=chgame.justPressed(SELECT_BUTTON);
    const bool left=chgame.repeat(LEFT_BUTTON,16,6), right=chgame.repeat(RIGHT_BUTTON,16,6);
    const bool up=chgame.repeat(UP_BUTTON,16,6), down=chgame.repeat(DOWN_BUTTON,16,6);
    if(scene.screen==view::Title) {
        if(left)scene.titleChoice=0;
        if(right)scene.titleChoice=1;
        if(start || a) {
            if(scene.titleChoice) { back=view::Title;screen(view::Help); }
            else { scene.heart=chgame.pressed(DOWN_BUTTON);screen(view::GameType);chip::effect(chip::Menu); }
        }
        else if(b) { back=view::Title; screen(view::Help); }
        else if(select) { scene.palette=(scene.palette+1)%3; view::palette(scene.palette); saveSettings(); }
        return;
    }
    if(scene.screen==view::Help || scene.screen==view::Records) {
        if(a || b || start)screen(back); return;
    }
    if(scene.screen==view::GameType) {
        if(left || right) { scene.type=right?1:0; chip::effect(chip::Menu); }
        if(a || down) screen(view::Music);
        if(start)screen(view::Level);
        if(b)screen(view::Title);
        return;
    }
    if(scene.screen==view::Music) {
        uint8_t previous=scene.music;
        if(left && (scene.music&1))--scene.music;
        if(right && !(scene.music&1))++scene.music;
        if(down && scene.music<2)scene.music+=2;
        if(up && scene.music>=2)scene.music-=2;
        if(previous!=scene.music) { music(); saveSettings(); }
        if(a || start)screen(view::Level);
        if(b)screen(view::GameType);
        return;
    }
    if(scene.screen==view::Level) {
        if(left && scene.level) --scene.level;
        if(right && scene.level<9) ++scene.level;
        if(up && scene.level>=5) scene.level-=5;
        if(down && scene.level<5) scene.level+=5;
        if(b)screen(view::GameType);
        if(select) { back=view::Level; screen(view::Records); }
        if(start)startGame();
        else if(a) { if(scene.type)screen(view::Height); else startGame(); }
        return;
    }
    if(scene.screen==view::Height) {
        if(left && scene.height)--scene.height;
        if(right && scene.height<5)++scene.height;
        if(up && scene.height>=3)scene.height-=3;
        if(down && scene.height<3)scene.height+=3;
        if(b)screen(view::Level);
        if(a || start)startGame();
        if(select) { back=view::Height; screen(view::Records); }
        return;
    }
    if(scene.screen==view::Pause) {
        if(b) { screen(view::GameType); music(); }
        else if(start || a) { screen(view::Play); chip::pause(scene.music==3); }
        else if(select) { scene.music=(scene.music+1)%4; music(); chip::pause(true); saveSettings(); }
        return;
    }
    if(scene.screen==view::Play) {
        if(start) { screen(view::Pause); chip::pause(true); return; }
        if(select) scene.preview=!scene.preview;
        game.frame(chgame.buttons());
        chip::setTempo(game.danger()?125:100);
        if(game.events & tetris::Four)chip::effect(chip::Tetris);
        else if(game.events & tetris::Line)chip::effect(chip::Clear);
        else if(game.events & tetris::Level)chip::effect(chip::LevelUp);
        else if(game.events & tetris::Locked)chip::effect(chip::Land);
        else if(game.events & tetris::Rotated)chip::effect(chip::Turn);
        else if(game.events & tetris::Moved)chip::effect(chip::Move);
        if(!finished && (game.phase==tetris::Won || game.phase==tetris::Lost))finishGame();
        return;
    }
    if(scene.screen==view::Result) {
        if(screenAge>60 && (a || start))records();
        return;
    }
    if(scene.screen==view::Name) {
        char &ch=scene.name[scene.namePosition];
        if(up)ch=ch=='Z'?' ':ch==' '?'A':ch+1;
        if(down)ch=ch=='A'?' ':ch==' '?'Z':ch-1;
        if(left && scene.namePosition)--scene.namePosition;
        if(right && scene.namePosition<5)++scene.namePosition;
        if(b && scene.namePosition)--scene.namePosition;
        if(start || (a && scene.namePosition==5)) {
            memcpy(saved.records[scene.type][scene.rank].name,scene.name,7);
            savePending=true; back=view::Level; screen(view::Records); music();
        } else if(a)++scene.namePosition;
    }
}

#if CHGAME_DEBUG
static bool debugState(char command,const char *args) {
    if(command=='F') {
        char state[72];
        snprintf(state,sizeof state,"TETRISFRAME %u %u\n",unsigned(scene.frame),unsigned(millis()));
        dbg::print(state);return true;
    }
    if(command=='H') {
        char state[180];
        snprintf(state,sizeof state,"TETRISSTATE %u %u %u %u %d %d %u %u %u %u %u %u %u %u %u %u\n",
                 scene.screen,game.phase,game.piece,game.rotation,game.x,game.y,game.next,game.level,
                 unsigned(game.score),game.lines,game.pieces,scene.preview,
                 unsigned(chip::ticks()),scene.music,scene.type,scene.palette);
        dbg::print(state);return true;
    }
    if(command=='V') {
        loadSettings(); scene.music=saved.music; scene.palette=saved.palette;
        view::palette(scene.palette); screen(view::Title); music();return true;
    }
    if(command=='W') {
        char board[200]="TETRISBOARD ";
        for(unsigned y=0;y<18;++y)for(unsigned x=0;x<10;++x)board[12+y*10+x]='0'+(game.board[y][x]&tetris::KIND_MASK);
        board[192]='\n';board[193]=0;
        dbg::print(board);return true;
    }
#ifdef CHSIM
    if(command=='J') {
        char line[224];
        unsigned used=snprintf(line,sizeof line,"TETRISSAVE %u %u ",saved.music,saved.palette);
        const auto *bytes=reinterpret_cast<const uint8_t *>(saved.records);
        const char *hex="0123456789abcdef";
        for(unsigned i=0;i<sizeof saved.records;++i) {
            line[used++]=hex[bytes[i]>>4];line[used++]=hex[bytes[i]&15];
        }
        line[used++]='\n';line[used]=0;dbg::print(line);return true;
    }
    if(command=='Z' && args && args[0]>='0' && args[0]<='3') {
        // Write current settings so restart checks exercise the real save
        // pages while retaining the current names/scores/palette.
        gfx_wait();saved.music=args[0]-'0';
        gameSaves.store(SAVE_ID,2,saved);gameSaves.store(SAVE_ID,2,saved);return true;
    }
    // Reproducible fixtures exercise real input, line animations and result UI.
    if(command=='Y') {
        if(!args || args[0]<'0' || args[0]>'9')return false;
        const unsigned fixture=args[0]-'0';
        scene.type=fixture==2; startGame();
        memset(game.board,0,sizeof game.board);
        game.piece=tetris::I; game.rotation=1; game.x=3; game.y=14;
        for(int row=14;row<18;++row)for(int col=0;col<10;++col)
            game.board[row][col]=col==4?0:1+(row+col)%7;
        if(fixture>=4) {
            const unsigned rows=fixture==8 ? (1u<<14)|(1u<<17) : ((1u<<(fixture-3))-1)<<(18-(fixture-3));
            memset(game.board,0,sizeof game.board);
            for(int row=14;row<18;++row)for(int col=0;col<10;++col)
                if(col!=4 && ((rows&(1u<<row)) || col==1 || col==8))
                    game.board[row][col]=1+(row+col)%7;
            // Floating markers make incorrect collapse order visible, including
            // separated full rows and the very top row of the field.
            game.board[0][0]=tetris::O+1;
            game.board[3][2]=tetris::T+1;
            game.board[8][6]=tetris::S+1;
            game.board[12][8]=tetris::J+1;
        }
        if(fixture==9) {
            memset(game.board,0,sizeof game.board);game.y=13;
            for(unsigned cy=0;cy<4;++cy) {
                game.board[14+cy][3]=tetris::lockedBlock(tetris::I,1,1,cy);
                game.board[14+cy][5]=tetris::lockedBlock(tetris::I,1,1,cy);
            }
        }
        if(fixture==1)game.lines=9;
        if(fixture==2)game.lines=21;
        if(fixture==3) { memset(game.board,1,sizeof game.board);game.spawn();finishGame(); }
        game.dropTimer=0; return true;
    }
#endif
    return false;
}
#endif

void setup() {
    chgame.boot(); dbg::begin("TETRIS 1.1.11");
#if CHGAME_DEBUG
    dbg::hook=debugState;
#endif
    gfx_begin(GFX_DIV2,GFX_12BPP); chgame.setFrameRate(60);
    loadSettings();
    scene.music=saved.music; scene.palette=saved.palette;
    view::palette(scene.palette); chip::begin(); music();
}
void loop() {
    dbg::poll();
#ifdef CHSIM
    while(simulatedAt!=millis()) {
        ++simulatedAt; chip::millisecond();
        unsigned n=(simulatedAt*125/4)-((simulatedAt-1)*125/4);
        while(n--)chip::sample();
    }
#endif
    if(!chgame.nextFrame())return;
    dbg::markUpdateStart();
    chgame.pollButtons(); ++scene.frame; ++screenAge;
    seed^=micros()+scene.frame; seed=(seed<<7)|(seed>>25);
    input();
    gfx_wait();
    if(savePending) { gameSaves.store(SAVE_ID,2,saved);savePending=false; }
    dbg::markRenderStart();
    view::draw(game,scene,saved.records[scene.type]);
    dbg::markRenderEnd();
    gfx_flushAsync();
}
