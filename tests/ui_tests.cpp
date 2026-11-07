#include "frontend.hpp"
#include <iostream>
#include <memory>
#include <fstream>
#define CHECK(c) do {if(!(c))throw std::runtime_error(#c);}while(0)
static void click(Frontend& ui,int x,int y) {SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;ui.event(e);}
static void key(Frontend& ui,SDL_Keycode k,int mod=0) {SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;e.key.keysym.mod=Uint16(mod);ui.event(e);}
int main(int,char**) {
    try {
        CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
        struct Quit {~Quit(){SDL_Quit();}} quit;
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> w(SDL_CreateWindow("TNS UI test",0,0,640,536,0),SDL_DestroyWindow);CHECK(w!=nullptr);
        std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> r(SDL_CreateRenderer(w.get(),-1,SDL_RENDERER_SOFTWARE),SDL_DestroyRenderer);CHECK(r!=nullptr);
        std::unique_ptr<Tns> m(new Tns);
        Frontend ui(*m,w.get(),r.get());CHECK(ui.running());
        click(ui,100,10);CHECK(ui.paused);click(ui,100,10);CHECK(ui.running());
        key(ui,SDLK_a);CHECK(m->key.isKey());CHECK(m->key.getKey()==97);
        key(ui,SDLK_F1);CHECK(m->key.getKey()==128);
        key(ui,SDLK_a,KMOD_CTRL|KMOD_ALT);CHECK(m->key.getKey()==1);
        click(ui,130,10);CHECK(!ui.running());click(ui,230,230);click(ui,510,435);CHECK(m->cfg.videoitk);CHECK(ui.running());
        click(ui,130,10);click(ui,160,145);click(ui,45,290);click(ui,510,435);CHECK(m->cfg.ramsize==256);CHECK(m->cfg.videobw);
        key(ui,SDLK_o,KMOD_CTRL|KMOD_ALT);CHECK(!ui.running());click(ui,420,115);
        // Exercise the actual SDL text event path and failed mount handling.
        SDL_Event text{};text.type=SDL_TEXTINPUT;std::strcpy(text.text.text,"missing-test.img");ui.event(text);key(ui,SDLK_RETURN);CHECK(!ui.running());
        key(ui,SDLK_ESCAPE);click(ui,510,445);CHECK(ui.running());
        key(ui,SDLK_h,KMOD_CTRL|KMOD_ALT);CHECK(!ui.running());ui.draw();key(ui,SDLK_ESCAPE);CHECK(ui.running());
        for(int i=0;i<100;++i) {m->frame();}
        ui.draw();
        std::vector<uint32_t> pixels(640*536);CHECK(SDL_RenderReadPixels(r.get(),nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),640*4)==0);
        CHECK((pixels[0]&0xffffff)==0xeeeeee);
        // Capture a reviewable real SDL rendering of the toolbar and emulator.
        std::ofstream f("ui.ppm",std::ios::binary);f<<"P6\n640 536\n255\n";for(auto p:pixels){char b[]={char(p>>16),char(p>>8),char(p)};f.write(b,3);}CHECK(bool(f));
        std::cout<<"PASS: SDL rendering, toolbar, pause, settings, keyboard, file dialog and help\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<' '<<SDL_GetError()<<'\n';return 1;}
}
