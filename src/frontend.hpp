#pragma once
#include <SDL.h>
#include "tns.hpp"
#include "icons.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#define tns_getcwd _getcwd
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define tns_getcwd getcwd
#endif
#include <fcntl.h>

struct FileEntry { std::string name; bool dir; };
inline std::vector<FileEntry> directory(const std::string& path) {
    std::vector<FileEntry> v;
#ifdef _WIN32
    WIN32_FIND_DATAW d; HANDLE h=FindFirstFileW(utf16(path+"/*").c_str(),&d);
    if(h!=INVALID_HANDLE_VALUE) {
        do { if(std::wstring(d.cFileName)!=L".") v.push_back({utf8(d.cFileName),bool(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)}); } while(FindNextFileW(h,&d));
        FindClose(h);
    }
#else
    DIR* d=opendir(path.c_str());
    if(d) { while(auto e=readdir(d)) {
        if(std::string(e->d_name)==".") continue;
        struct stat s; bool isdir=!stat((path+"/"+e->d_name).c_str(),&s) && S_ISDIR(s.st_mode);
        v.push_back({e->d_name,isdir});
    } closedir(d); }
#endif
    std::sort(v.begin(),v.end(),[](const FileEntry& a,const FileEntry& b){return a.dir!=b.dir?a.dir>b.dir:a.name<b.name;});
    return v;
}
inline std::string cwd() {
#ifdef _WIN32
    wchar_t p[4096];return _wgetcwd(p,4096)?utf8(p):".";
#else
    char p[4096];return getcwd(p,sizeof p)?p:".";
#endif
}
inline bool absolute(const std::string& s) { return !s.empty() && (s[0]=='/' || s[0]=='\\' || (s.size()>1 && s[1]==':')); }
inline bool createImage(const std::string& path,int64_t size) {
#ifdef _WIN32
    int fd=_wopen(utf16(path).c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
    FILE* f=fd<0?nullptr:_fdopen(fd,"wb");
#else
    int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
    FILE* f=fd<0?nullptr:fdopen(fd,"wb");
#endif
    if(!f) {
#ifdef _WIN32
        if(fd>=0)_close(fd);
#else
        if(fd>=0)::close(fd);
#endif
        return false;
    }
    uint8_t block[8192]; std::fill(block,block+sizeof block,0xe5);
    bool ok=true;
    while(size>0) { size_t n=size_t(std::min<int64_t>(size,sizeof block)); if(std::fwrite(block,1,n,f)!=n) {ok=false;break;} size-=n; }
    if(std::fclose(f)) ok=false;
    if(!ok) removeFile(path);
    return ok;
}

// A small SDL-only UI keeps the executable independent of Qt/GTK/font/image libraries.
class Frontend {
    Tns& m; SDL_Window* window; SDL_Renderer* r;
    SDL_Texture* screen=nullptr;
    std::vector<SDL_Texture*> icons;
    enum Panel { NONE,DRIVES,SETTINGS,BROWSER,HELP } panel=NONE;
    Config pending;
    int selected=0,first=0,drive=0;
    bool creating=false,textFocus=true,selectAll=false;
    std::string folder=cwd(),input,message;
    std::vector<FileEntry> entries;
    const int ramValues[5]={64,256,512,768,1024};
    void color(uint32_t c) { SDL_SetRenderDrawColor(r,uint8_t(c>>16),uint8_t(c>>8),uint8_t(c),255); }
    void rect(int x,int y,int w,int h,uint32_t c) { color(c); SDL_Rect q={x,y,w,h};SDL_RenderFillRect(r,&q); }
    // Use the original 6x12 ITK font for dialog text, with no font dependency.
    void text(int x,int y,std::string s,uint32_t c=0x202020,size_t max=90) {
        if(s.size()>max) s="..."+s.substr(s.size()-max+3);
        color(c);
        for(unsigned char ch:s) {
            if(ch>=128) ch='?';
            for(int a=0;a<12;++a) { int b=m.mem.readIChar(ch*12+a);for(int bit=0;bit<6;++bit) if(b&(32>>bit)) SDL_RenderDrawPoint(r,x+bit,y+a); }
            x+=6;
        }
    }
    bool hit(int x,int y,int bx,int by,int w,int h) { return x>=bx&&x<bx+w&&y>=by&&y<by+h; }
    void button(int x,int y,int w,const std::string& s,bool active=false) {
        rect(x,y,w,24,active?0xc0d5eb:0xf2f2f2);
        color(0x8b8b8b); SDL_Rect b={x,y,w,24};SDL_RenderDrawRect(r,&b);text(x+6,y+6,s);
    }
    void base(const std::string& title) {
        rect(20,66,600,410,0xe8e8e8);rect(20,66,600,26,0xd2d9e2);text(32,74,title);
    }
    void browse(int n,bool create) {
        drive=n;creating=create;panel=BROWSER;message.clear();selected=first=0;textFocus=true;selectAll=true;
        input=create?(n==4?"new.hdd":"new.img"):(n==4?m.cfg.hdd:m.cfg.drives[n]);
        if(!input.empty() && !create) {
            auto p=input.find_last_of("/\\"); if(p!=std::string::npos) {folder=input.substr(0,p);input=input.substr(p+1);}
        }
        if(folder.empty()) folder="/";
        entries=directory(folder);SDL_StartTextInput();
    }
    void openEntry(int n) {
        if(n<0 || n>=int(entries.size())) return;
        auto e=entries[size_t(n)];
        if(e.dir) {
            if(e.name=="..") { auto p=folder.find_last_of("/\\");folder=p==std::string::npos?cwd():(p==0?"/":folder.substr(0,p)); }
            else { if(folder.back()!='/') folder+='/';folder+=e.name; }
            entries=directory(folder);selected=first=0;
        } else { input=e.name;selectAll=true;textFocus=true; }
    }
    void accept() {
        if(input.empty()) { message="Enter an image filename.";return; }
        std::string path=absolute(input)?input:folder+"/"+input;
        if(drive<4 && m.wdc.isInUse()==drive+1) {message="Drive in use. Deselect it in the emulated system first.";return;}
        if(creating) {
            int64_t size=drive==4?2610LL*8192:int64_t(m.cfg.geometry[drive].bps)*m.cfg.geometry[drive].sectors*m.cfg.geometry[drive].tracks;
            if(!createImage(path,size)) {message="Cannot create image (existing files are preserved).";return;}
        }
        bool ok=drive==4?m.wap.insertImage(path):m.wdc.insertImage(drive+1,path);
        if(!ok) { message="Cannot mount image. Check path, access and active drive.";return; }
        SDL_StopTextInput();panel=DRIVES;message.clear();
    }
    void edit(const std::string& s) { if(selectAll) {input.clear();selectAll=false;}if(input.size()+s.size()<=4096) input+=s; }
public:
    bool paused=false,quit=false;
    Frontend(Tns& machine,SDL_Window* w,SDL_Renderer* renderer):m(machine),window(w),r(renderer) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
        SDL_RenderSetLogicalSize(r,640,536);
        screen=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,256);
        if(!screen) throw std::runtime_error(SDL_GetError());
        const Icon* images[]={&icon_open,&icon_reset,&icon_nmi,&icon_pause,&icon_run,&icon_settings,&icon_yellow};
        for(auto i:images) {
            SDL_Texture* t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,i->w,i->h);
            if(!t) throw std::runtime_error(SDL_GetError());
            SDL_UpdateTexture(t,nullptr,i->pixels,i->w*4);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);icons.push_back(t);
        }
        SDL_EventState(SDL_DROPFILE,SDL_ENABLE);
    }
    Frontend(const Frontend&)=delete;
    Frontend& operator=(const Frontend&)=delete;
    ~Frontend() { for(auto t:icons) SDL_DestroyTexture(t);SDL_DestroyTexture(screen);SDL_StopTextInput(); }
    bool running() const { return !paused && panel==NONE; }
    void draw() {
        color(0);SDL_RenderClear(r);
        SDL_UpdateTexture(screen,nullptr,m.image.pixels.data(),640*4);
        SDL_Rect dst={0,24,640,512};SDL_RenderCopy(r,screen,nullptr,&dst);
        rect(0,0,640,24,0xeeeeee);
        int idx[]={0,1,2,paused?4:3,5};
        for(int n=0;n<5;++n) {
            SDL_Rect d={n*30+2,2,20,20};SDL_RenderCopy(r,icons[idx[n]],nullptr,&d);
            rect(n*30+26,3,1,18,0xc6c6c6);
        }
        for(int n=0;n<4;++n) {
            SDL_SetTextureColorMod(icons[6],m.wdc.isInUse()==n+1?255:120,m.wdc.isInUse()==n+1?255:120,m.wdc.isInUse()==n+1?255:120);
            SDL_Rect d={158+n*32,2,20,20};SDL_RenderCopy(r,icons[6],nullptr,&d);
        }
        if(panel==SETTINGS) {
            base("Settings");text(40,115,"Main RAM");
            for(int i=0;i<5;++i) button(40+i*112,140,100,std::to_string(ramValues[i])+" KB",pending.ramsize==ramValues[i]);
            text(40,200,"Video card");button(40,225,160,"BGD / GC",!pending.videoitk);button(215,225,160,"ITK / SC",pending.videoitk);
            button(40,285,240,pending.videobw?"[X] Video BW":"[ ] Video BW");
            text(40,345,"Changing RAM or card resets the computer.");
            button(400,428,90,"Cancel");button(505,428,90,"OK");
        } else if(panel==DRIVES) {
            base("Disk drives");
            for(int n=0;n<5;++n) {
                int y=110+n*57;text(34,y+6,n==4?"H":std::to_string(n+1));
                rect(52,y,357,24,0xffffff);text(58,y+6,n==4?m.cfg.hdd:m.cfg.drives[n],0x202020,56);
                button(416,y,55,"Open");button(478,y,55,"Eject");button(540,y,55,"New");
                if(n<4) text(58,y+30,std::to_string(m.cfg.geometry[n].tracks)+" tracks, "+std::to_string(m.cfg.geometry[n].sectors)+" sectors, "+std::to_string(m.cfg.geometry[n].bps)+" bytes");
            }
            text(34,408,message,0xa02020,91);button(505,438,90,"OK");
        } else if(panel==BROWSER) {
            base(creating?"New disk image":"Open disk image");text(35,101,folder,0x202020,92);
            rect(34,120,570,234,0xffffff);
            for(int j=0;j<16 && first+j<int(entries.size());++j) {
                int n=first+j,y=122+j*14;
                if(n==selected) rect(35,y,568,14,0xc5dbf0);
                text(40,y+1,(entries[n].dir?"[DIR] ":"      ")+entries[n].name,0x202020,90);
            }
            text(35,365,"Filename (type or paste a full path):");
            rect(34,383,570,26,selectAll?0xc5dbf0:0xffffff);text(40,390,input+(textFocus?"_":""),0x202020,91);
            text(35,416,message,0xa02020,91);
            button(310,438,85,"Up");button(405,438,90,"Cancel");button(505,438,90,creating?"Create":"Open");
        } else if(panel==HELP) {
            base("TNS emulator - controls");
            const char* lines[]={"Toolbar: disks, reset, NMI, pause/run, settings; four drive LEDs.","Ctrl+Alt+O  Disk drives     Ctrl+Alt+S  Settings","Ctrl+Alt+R  Reset           Ctrl+Alt+N  NMI","Ctrl+Alt+P  Pause/run       Ctrl+Alt+Enter  Fullscreen","Ctrl+Alt+H  This help       Esc  Close a dialog","Drop a disk image to open it in the drive dialog.","F1..F12 and the numeric keypad are sent to TNS.","Scroll Lock switches the extended function key bank.","New images are filled with E5, as in the Java emulator.","Images can be written by the emulated computer."};
            for(int n=0;n<10;++n) text(34,112+n*25,lines[n]);
            button(505,438,90,"OK");
        }
        SDL_RenderPresent(r);
    }
    void keyboard(const SDL_KeyboardEvent& e) {
        SDL_Keycode k=e.keysym.sym;auto mod=SDL_Keymod(e.keysym.mod);
        bool shift=bool(mod&KMOD_SHIFT),ctrl=bool(mod&KMOD_CTRL),alt=bool(mod&KMOD_ALT);
        if(ctrl&&alt) {
            bool shortcut=true;
            switch(k) {
                case SDLK_r:m.Reset(false);break;case SDLK_n:m.Nmi();break;case SDLK_p:paused=!paused;break;
                case SDLK_o:panel=DRIVES;break;case SDLK_s:pending=m.cfg;panel=SETTINGS;break;case SDLK_h:panel=HELP;break;
                case SDLK_RETURN:SDL_SetWindowFullscreen(window,(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)?0:SDL_WINDOW_FULLSCREEN_DESKTOP);break;
                default:shortcut=false;break;
            }
            if(shortcut)return;
        }
        if(panel!=NONE) {
            if(k==SDLK_ESCAPE) { panel=panel==BROWSER?DRIVES:NONE;SDL_StopTextInput();message.clear();return; }
            if(panel==BROWSER) {
                if(k==SDLK_RETURN) {if(!textFocus && !entries.empty()) openEntry(selected);else accept();}
                else if(k==SDLK_TAB) {textFocus=!textFocus;selectAll=textFocus;}
                else if(k==SDLK_DOWN || k==SDLK_UP) {textFocus=false;selected=std::max(0,std::min(int(entries.size())-1,selected+(k==SDLK_DOWN?1:-1)));if(selected<first)first=selected;if(selected>=first+16)first=selected-15;}
                else if(ctrl&&k==SDLK_a) selectAll=true;
                else if(ctrl&&k==SDLK_v) {char* p=SDL_GetClipboardText();if(p){edit(p);SDL_free(p);}}
                else if(k==SDLK_BACKSPACE && textFocus) {
                    if(selectAll) {input.clear();selectAll=false;}
                    else if(!input.empty()) { size_t p=input.size()-1;while(p>0 && (uint8_t(input[p])&0xc0)==0x80)--p;input.erase(p); }
                }
            }
            return;
        }
        int code=0;bool action=false,numpad=false;
        if(k>=SDLK_a && k<=SDLK_z) code=int(k)-'a'+'A';
        else if(e.keysym.scancode>=SDL_SCANCODE_1 && e.keysym.scancode<=SDL_SCANCODE_9) code='1'+int(e.keysym.scancode-SDL_SCANCODE_1);
        else if(e.keysym.scancode==SDL_SCANCODE_0) code='0';
        else if(k>=32 && k<127) code=int(k);
        else if(k>=SDLK_F1 && k<=SDLK_F12) {code=112+int(k-SDLK_F1);action=true;}
        else if(k>=SDLK_F13 && k<=SDLK_F20) {code=124+int(k-SDLK_F13);action=true;}
        else switch(k) {
            case 0x016f:code=999;break;case 0x00a7:code=58;break;case 0x00fa:code=64;break;case 0x00a8:code=93;break;
            case SDLK_RETURN:code=10;break;case SDLK_BACKSPACE:code=8;break;case SDLK_TAB:code=9;break;
            case SDLK_ESCAPE:code=27;break;case SDLK_DELETE:code=127;break;
            case SDLK_INSERT:code=92;action=true;break;
            case SDLK_LEFT:code=37;action=true;break;case SDLK_UP:code=38;action=true;break;case SDLK_RIGHT:code=39;action=true;break;case SDLK_DOWN:code=40;action=true;break;
            case SDLK_HOME:code=36;action=true;break;case SDLK_END:code=35;action=true;break;case SDLK_PAGEUP:code=33;action=true;break;case SDLK_PAGEDOWN:code=34;action=true;break;
            case SDLK_KP_0:code=96; numpad=true;break;case SDLK_KP_1:code=97;numpad=true;break;case SDLK_KP_2:code=98;numpad=true;break;case SDLK_KP_3:code=99;numpad=true;break;case SDLK_KP_4:code=100;numpad=true;break;case SDLK_KP_5:code=101;numpad=true;break;case SDLK_KP_6:code=102;numpad=true;break;case SDLK_KP_7:code=103;numpad=true;break;case SDLK_KP_8:code=104;numpad=true;break;case SDLK_KP_9:code=105;numpad=true;break;
            case SDLK_KP_MULTIPLY:code=106;numpad=true;break;case SDLK_KP_PLUS:code=107;numpad=true;break;case SDLK_KP_MINUS:code=109;numpad=true;break;case SDLK_KP_PERIOD:code=110;numpad=true;break;case SDLK_KP_DIVIDE:code=111;numpad=true;break;case SDLK_KP_ENTER:code=10;numpad=true;break;
            default:return;
        }
        if(k==SDLK_QUOTE) code=222;
        if(k==SDLK_SEMICOLON) code=999;
        // Java exposes physical alphabetic/digit keycodes; retain its TNS table.
        if(numpad && !(mod&KMOD_NUM)) {
            switch(k) {case SDLK_KP_0:code=92;break;case SDLK_KP_1:code=35;break;case SDLK_KP_2:code=40;break;case SDLK_KP_3:code=34;break;case SDLK_KP_4:code=37;break;case SDLK_KP_5:code=41;break;case SDLK_KP_6:code=39;break;case SDLK_KP_7:code=36;break;case SDLK_KP_8:code=38;break;case SDLK_KP_9:code=33;break;default:break;}
        }
        m.key.press(code,shift,ctrl,alt,action,numpad,bool(mod&KMOD_CAPS),scrollLock);
    }
    bool scrollLock=false;
    void event(const SDL_Event& e) {
        if(e.type==SDL_QUIT) quit=true;
        if(e.type==SDL_KEYDOWN) {if(e.key.keysym.sym==SDLK_SCROLLLOCK && !e.key.repeat)scrollLock=!scrollLock;else keyboard(e.key);}
        if(e.type==SDL_TEXTINPUT && panel==BROWSER && textFocus) edit(e.text.text);
        if(e.type==SDL_DROPFILE) {
            std::string path=e.drop.file;SDL_free(e.drop.file);
            if(panel!=BROWSER) browse(0,false);
            input=path;selectAll=true;message="Choose Open to mount this image.";
        }
        if(e.type==SDL_MOUSEWHEEL && panel==BROWSER) first=std::max(0,std::min(std::max(0,int(entries.size())-16),first-e.wheel.y*3));
        if(e.type!=SDL_MOUSEBUTTONDOWN || e.button.button!=SDL_BUTTON_LEFT) return;
        // SDL already transforms mouse events into logical renderer coordinates.
        int x=e.button.x,y=e.button.y;
        if(panel==NONE) {
            if(y<24) {
                if(x<30) panel=DRIVES;else if(x<60)m.Reset(false);else if(x<90)m.Nmi();else if(x<120)paused=!paused;else if(x<150){pending=m.cfg;panel=SETTINGS;}
            }
        } else if(panel==SETTINGS) {
            for(int i=0;i<5;++i) if(hit(x,y,40+i*112,140,100,24)) pending.ramsize=ramValues[i];
            if(hit(x,y,40,225,160,24)) pending.videoitk=false;
            if(hit(x,y,215,225,160,24)) pending.videoitk=true;
            if(hit(x,y,40,285,240,24)) pending.videobw=!pending.videobw;
            if(hit(x,y,400,428,90,24)) panel=NONE;
            if(hit(x,y,505,428,90,24)) {
                bool reset=pending.ramsize!=m.cfg.ramsize || pending.videoitk!=m.cfg.videoitk;
                m.cfg=pending; m.grf.reSetPalette();if(reset)m.Reset(false);panel=NONE;
            }
        } else if(panel==DRIVES) {
            for(int n=0;n<5;++n) {
                int yy=110+n*57;
                if(hit(x,y,416,yy,55,24))browse(n,false);
                else if(hit(x,y,540,yy,55,24))browse(n,true);
                else if(hit(x,y,478,yy,55,24)) {
                    if(n==4)m.wap.ejectImage();else if(!m.wdc.ejectImage(n+1))message="Drive in use; cannot eject.";
                }
            }
            if(hit(x,y,505,438,90,24))panel=NONE;
        } else if(panel==BROWSER) {
            if(hit(x,y,34,383,570,26)) {textFocus=true;selectAll=true;}
            if(hit(x,y,34,120,570,234)) {selected=first+(y-122)/14;textFocus=false;openEntry(selected);}
            if(hit(x,y,310,438,85,24)) {entries.insert(entries.begin(),{"..",true});openEntry(0);}
            if(hit(x,y,405,438,90,24)) {panel=DRIVES;SDL_StopTextInput();}
            if(hit(x,y,505,438,90,24)) accept();
        } else if(panel==HELP && hit(x,y,505,438,90,24))panel=NONE;
    }
};
