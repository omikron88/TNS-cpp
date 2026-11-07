#include "frontend.hpp"
#include <iostream>
#include <fstream>
#include <memory>
#include <cstdlib>
#include <limits>

static int number(const std::string& v) {
    size_t end=0;int n=std::stoi(v,&end);if(end!=v.size())throw std::runtime_error("Invalid number: "+v);return n;
}
static void screenshot(const Image& img,const std::string& path) {
    FILE* f=openFile(path,"wb");if(!f)throw std::runtime_error("Cannot create screenshot: "+path);
    bool ok=std::fprintf(f,"P6\n640 256\n255\n")>0;
    for(auto p:img.pixels) {unsigned char b[]={uint8_t(p>>16),uint8_t(p>>8),uint8_t(p)};if(std::fwrite(b,1,3,f)!=3)ok=false;}
    if(std::fclose(f))ok=false;
    if(!ok)throw std::runtime_error("Cannot write screenshot: "+path);
}
static void trace(Tns& m,int frame) {
    uint32_t h=2166136261u;for(auto p:m.image.pixels)h=(h^p)*16777619u;
    std::cout<<"TRACE "<<frame<<' '<<m.cpu.getRegPC()<<' '<<m.cpu.getRegAF()<<' '<<m.cpu.getRegBC()<<' '<<m.cpu.getRegDE()<<' '<<m.cpu.getRegHL()<<' '<<m.cpu.getRegSP()<<' '<<m.clk.getTstates()<<' '<<int32_t(m.mem.checksum())<<' '<<int32_t(h)<<'\n';
}
static void help() {
    std::cout<<"TNS C++11 / SDL2 - port of omikron88/TNS (GPL-3.0)\n"
      "Usage: tns [options]\n"
      "  --itk | --gc           ITK/SC or BGD/GC video card\n"
      "  --bw | --color         Monochrome or color palette\n"
      "  --ram KIB              RAM size, 64..1024, multiples of 64\n"
      "  --drive1 FILE ... --drive4 FILE    Mount floppy images (written in place)\n"
      "  --hdd FILE             Mount WAP HDD image (written in place)\n"
      "  --config FILE          Configuration path (default: ./tns.config)\n"
      "  --no-save              Do not save configuration on exit\n"
      "  --rom-dir DIR          Override all four embedded ROMs with original-size or 8192-byte files\n"
      "  --paused               Start paused\n"
      "  --frames N             Run N frames without real-time throttling, then exit\n"
      "  --headless             Run without a window (requires --frames)\n"
      "  --trace                Print per-frame CPU, clock, RAM and raster checksums\n"
      "  --screenshot FILE      Save final native 640x256 raster as PPM\n"
      "  --help                 This help\n"
      "Toolbar: disks, reset, NMI, pause/run, settings; drive LEDs.\n"
      "Ctrl+Alt+O/R/N/P/S/H: disks/reset/NMI/pause/settings/help.\n"
      "Ctrl+Alt+Enter: fullscreen. Original function keys go to TNS.\n";
}
int main(int argc,char** argv) {
    try {
        std::string config="tns.config",romdir,shot;bool save=true,paused=false,headless=false,tracing=false;int frames=0;
        for(int i=1;i<argc;++i) {
            std::string a=argv[i];
            if(a=="--help" || a=="-h") {help();return 0;}
            if(a=="--config") {if(++i>=argc)throw std::runtime_error("Missing --config path");config=argv[i];}
        }
        Config cfg;cfg.load(config);
        for(int i=1;i<argc;++i) {
            std::string a=argv[i];
            auto value=[&]()->std::string { if(++i>=argc)throw std::runtime_error("Missing value for "+a);return argv[i]; };
            if(a=="--config") value();
            else if(a=="--itk")cfg.videoitk=true;else if(a=="--gc")cfg.videoitk=false;
            else if(a=="--bw")cfg.videobw=true;else if(a=="--color")cfg.videobw=false;
            else if(a=="--ram")cfg.ramsize=number(value());
            else if(a.size()==8 && a.substr(0,7)=="--drive" && a[7]>='1' && a[7]<='4')cfg.drives[a[7]-'1']=value();
            else if(a=="--hdd")cfg.hdd=value();
            else if(a=="--rom-dir")romdir=value();else if(a=="--screenshot")shot=value();
            else if(a=="--no-save")save=false;else if(a=="--paused")paused=true;
            else if(a=="--headless")headless=true;else if(a=="--trace")tracing=true;
            else if(a=="--frames") {frames=number(value());if(frames<1)throw std::runtime_error("--frames must be positive");}
            else throw std::runtime_error("Unknown option: "+a);
        }
        cfg.validate();
        if(headless && !frames)throw std::runtime_error("--headless requires --frames");
        if(frames && paused)throw std::runtime_error("--frames cannot be combined with --paused");
        for(const auto& p:cfg.drives) if(!p.empty()) {DiskFile probe;if(!probe.open(p))throw std::runtime_error("Cannot open floppy image: "+p);}
        if(!cfg.hdd.empty()) {DiskFile probe;if(!probe.open(cfg.hdd))throw std::runtime_error("Cannot open HDD image: "+cfg.hdd);}
        // Heap allocation also works with the small default Windows stack.
        std::unique_ptr<Tns> machine(new Tns(cfg,romdir));Tns& m=*machine;
        if(headless) {
            for(int n=0;n<frames;++n) {m.frame();if(tracing)trace(m,n);}
        } else {
            if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS)!=0)throw std::runtime_error(SDL_GetError());
            struct Quit {~Quit(){SDL_Quit();}} quit;
            std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow(cfg.videoitk?"TNS SC":"TNS GC",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,536,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),SDL_DestroyWindow);
            if(!window)throw std::runtime_error(SDL_GetError());
            SDL_SetWindowMinimumSize(window.get(),640,536);
            SDL_Renderer* raw=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED);
            if(!raw)raw=SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE);
            std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(raw,SDL_DestroyRenderer);
            if(!renderer)throw std::runtime_error(SDL_GetError());
            Frontend ui(m,window.get(),renderer.get());ui.paused=paused;
            int n=0;uint64_t deadline=uint64_t(SDL_GetTicks());
            while(!ui.quit) {
                SDL_Event e;while(SDL_PollEvent(&e))ui.event(e);
                bool run=ui.running();
                if(run) {m.frame();if(tracing)trace(m,n);++n;}
                SDL_SetWindowTitle(window.get(),(std::string(m.cfg.videoitk?"TNS SC":"TNS GC")+(ui.paused?" - paused":"")).c_str());
                ui.draw();
                if(frames && n>=frames)break;
                if(!frames) {
                    if(!run) deadline=uint64_t(SDL_GetTicks());
                    deadline+=20;uint64_t now=uint64_t(SDL_GetTicks());
                    if(deadline>now)SDL_Delay(Uint32(deadline-now));else if(now-deadline>100)deadline=now;
                }
            }
        }
        if(!shot.empty())screenshot(m.image,shot);
        if(save && !m.cfg.save(config)) {std::cerr<<"Cannot save configuration: "<<config<<'\n';return 1;}
        return 0;
    } catch(const std::exception& e) {std::cerr<<"TNS: "<<e.what()<<'\n';return 1;}
}
