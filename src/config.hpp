#pragma once
#include <array>
#include <string>
#include "files.hpp"
#include <sstream>
#include <stdexcept>
struct Config {
    int ramsize = 256;
    bool videobw = false, videoitk = false;
    std::array<std::string,4> drives;
    std::string hdd;
    struct Geometry { int bps=128, sectors=26, tracks=77, sides=1; };
    std::array<Geometry,4> geometry;
    void validate() const {
        if(ramsize<64 || ramsize>1024 || ramsize%64) throw std::runtime_error("RAM must be 64..1024 KiB in steps of 64");
        for(const auto& g:geometry)
            if(g.bps<128 || g.bps>1024 || (g.bps&(g.bps-1)) || g.sectors<1 || g.sectors>255 || g.tracks<1 || g.tracks>255 || g.sides!=1)
                throw std::runtime_error("Invalid disk geometry (128/256/512/1024 bytes, 1..255 sectors/tracks, one side)");
    }
    void load(const std::string& path) {
        FILE* source=openFile(path,"rb");if(!source)return;
        std::string contents;char block[4096];size_t n;
        while((n=std::fread(block,1,sizeof block,source)))contents.append(block,n);
        bool failed=bool(std::ferror(source));std::fclose(source);
        if(failed)throw std::runtime_error("Cannot read configuration: "+path);
        std::istringstream f(contents);std::string line;
        while(std::getline(f,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            auto p=line.find('='); if(p==std::string::npos || line[0]=='#') continue;
            std::string k=line.substr(0,p), v=line.substr(p+1);
            if(k=="RAMSIZE") ramsize=std::stoi(v);
            else if(k=="VIDEOITK") videoitk=(v=="true" || v=="1");
            else if(k=="VIDEOBW") videobw=(v=="true" || v=="1");
            else if(k=="HDD") hdd=v;
            else for(int i=0;i<4;++i) {
                std::string n=std::to_string(i+1);
                if(k=="DRIVE"+n) drives[i]=v;
                if(k=="BPS"+n) geometry[i].bps=std::stoi(v);
                if(k=="SECTORS"+n) geometry[i].sectors=std::stoi(v);
                if(k=="TRACKS"+n) geometry[i].tracks=std::stoi(v);
            }
        }
        validate();
    }
    bool save(const std::string& path) const {
        std::ostringstream f;
        f<<"# TNS C++ configuration\nRAMSIZE="<<ramsize<<"\nVIDEOITK="<<(videoitk?"true":"false")<<"\nVIDEOBW="<<(videobw?"true":"false")<<"\nHDD="<<hdd<<'\n';
        for(int i=0;i<4;++i) f<<"DRIVE"<<i+1<<'='<<drives[i]<<"\nBPS"<<i+1<<'='<<geometry[i].bps<<"\nSECTORS"<<i+1<<'='<<geometry[i].sectors<<"\nTRACKS"<<i+1<<'='<<geometry[i].tracks<<'\n';
        std::string data=f.str();FILE* target=openFile(path,"wb");if(!target)return false;
        bool ok=std::fwrite(data.data(),1,data.size(),target)==data.size();if(std::fclose(target))ok=false;return ok;
    }
};
