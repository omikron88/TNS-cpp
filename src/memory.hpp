#pragma once
#include "config.hpp"
#include "roms.hpp"
#include <array>
#include <algorithm>
#include <fstream>
#include <cstdint>
class Memory {
    Config& cf;
    typedef std::array<uint8_t,8192> Page;
    std::array<Page,128> ram{};
    std::array<Page,8> vram{};
    Page bootG{},bootS{},chars{},ichars{},fake{},sink{};
    std::array<uint8_t*,128> reads{},writes{};
    void load(Page& page,const unsigned char* data,size_t size,const std::string& dir,const char* name) {
        if(size>page.size())throw std::runtime_error("Embedded ROM exceeds page size");
        std::copy(data,data+size,page.begin());
        if(!dir.empty()) {
            FILE* f=openFile(dir+"/"+name,"rb");
            if(!f)throw std::runtime_error(std::string("Cannot open ROM: ")+name);
            size_t count=std::fread(page.data(),1,page.size(),f);
            bool invalid=(count!=size && count!=8192) || std::fgetc(f)!=EOF || std::ferror(f);
            std::fclose(f);
            if(invalid)throw std::runtime_error(std::string("Invalid ROM length: ")+name);
        }
    }
public:
    explicit Memory(Config& config,const std::string& dir="") : cf(config) {
        cf.validate();
        load(bootG,rom_bootGC,sizeof rom_bootGC,dir,"bootGC.bin");
        load(bootS,rom_bootSC,sizeof rom_bootSC,dir,"bootSC.bin");
        load(chars,rom_chars,sizeof rom_chars,dir,"chars.bin");
        load(ichars,rom_charsITK,sizeof rom_charsITK,dir,"charsITK.bin");
        reset(true);
    }
    Memory(const Memory&)=delete;
    Memory& operator=(const Memory&)=delete;
    void reset(bool dirty) {
        if(dirty) {
            int c=0,a=0;
            for(int i=0;i<128;++i) for(int j=0;j<8192;++j) {
                ram[i][j]=uint8_t(c); if(i<8) vram[i][j]=uint8_t(c);
                a=(a+1)&127; if(a==0) c^=255;
            }
        }
        fake.fill(255);
        for(int i=0;i<128;++i) { reads[i]=fake.data(); writes[i]=sink.data(); }
        for(int i=0;i<(cf.ramsize>>3);++i) reads[i]=writes[i]=ram[i].data();
        if(!cf.videoitk) for(int i=0;i<8;++i) reads[112+i]=writes[112+i]=vram[i].data();
    }
    int readByte(int a) const { a&=0xfffff; return reads[a>>13][a&8191]; }
    void writeByte(int a,int v) { a&=0xfffff; writes[a>>13][a&8191]=uint8_t(v); }
    int readVram(int a) const { return vram[(a&65535)>>13][a&8191]; }
    int readChar(int a) const { return chars[a&8191]; }
    int readIChar(int a) const { return ichars[a&8191]; }
    void bootRom(bool on) { reads[0]=on?(cf.videoitk?bootS.data():bootG.data()):writes[0]; }
    uint32_t checksum() const {
        uint32_t h=2166136261u;
        for(const auto& p:ram) for(auto b:p) h=(h^b)*16777619u;
        for(const auto& p:vram) for(auto b:p) h=(h^b)*16777619u;
        return h;
    }
};
