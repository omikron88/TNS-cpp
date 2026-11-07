#pragma once
#include "config.hpp"
#include "bus.hpp"
#include <array>
#include <cstdio>
#include <algorithm>
#include <memory>

// Each mounted image owns its handle. Reads never create a missing image.
class DiskFile {
    FILE* file=nullptr;
public:
    bool readonly=false;
    DiskFile()=default;
    DiskFile(const DiskFile&)=delete;
    DiskFile& operator=(const DiskFile&)=delete;
    ~DiskFile() { close(); }
    void close() { if(file) std::fclose(file); file=nullptr; readonly=false; }
    bool open(const std::string& path) {
        close(); if(path.empty()) return false;
        file=openFile(path,"r+b");
        if(!file) { file=openFile(path,"rb"); readonly=bool(file); }
        return bool(file);
    }
    bool ready() const { return file!=nullptr; }
    bool read(int64_t offset,uint8_t* data,size_t size) {
        std::fill(data,data+size,0);
        return file && offset>=0 && offset<=0x7fffffff && !std::fseek(file,long(offset),SEEK_SET) && std::fread(data,1,size,file)==size;
    }
    bool write(int64_t offset,const uint8_t* data,size_t size) {
        return file && !readonly && offset>=0 && offset<=0x7fffffff && !std::fseek(file,long(offset),SEEK_SET) && std::fwrite(data,1,size,file)==size && !std::fflush(file);
    }
};
class Wap {
    Config& cfg;
    DiskFile f;
    std::array<uint8_t,5> cmd{};
    std::array<uint8_t,128> bb{};
    int del=0,pos=0,cpos=0,st=0,sek=0;
    bool exec=false,good=true;
public:
    explicit Wap(Config& c):cfg(c) { reset(); }
    void reset() { pos=cpos=del=0; exec=false; good=true; f.open(cfg.hdd); }
    int isRInt() const { return f.ready()&&good?1:0; }
    int isWInt() const { return f.ready()&&good?1:0; }
    bool insertImage(const std::string& path) { if(!f.open(path)) return false; cfg.hdd=path; return true; }
    void ejectImage() { f.close(); cfg.hdd.clear(); }
    void wapOut(int val) {
        if(!f.ready()) return;
        if(exec) {
            if(del>0) {
                bb[pos++]=uint8_t(val); --del;
                if(!del) { exec=false; if(cmd[0]==4) good=f.write(8192LL*st+128*sek,bb.data(),128); }
            } else exec=false;
        } else {
            cmd[cpos++]=uint8_t(val);
            st=cmd[2]+256*cmd[3]; sek=cmd[4];
            if(cmd[0]==1) cpos=0;
            if(cpos==5 && (cmd[0]==3 || cmd[0]==4)) {
                del=128; pos=0; exec=true;
                if(cmd[0]==3) good=f.read(8192LL*st+128*sek,bb.data(),128);
            }
            if(cpos>4) cpos=0;
        }
    }
    int wapIn() {
        if(!f.ready()) return 0;
        if(del>0) { --del; return bb[pos++]; }
        exec=false; return 0;
    }
};
class Wd {
    Config& cfg; Clock& clk;
    DiskFile f;
    int open=0;
    Config::Geometry d;
    int trk=0,sec=1,ctl=0,res=4,dat=0,stat=0,cnt=0,timeout=0,mode=0,r3212=0;
    int64_t position=0;
    bool ir=false;
    std::array<uint8_t,1024> bb{},ring{};
    size_t rp=0;
    enum { IDLE,READ,WRITE,HEAD,SEEK };
    enum { NRDY=128,WINH=64,NFND=16,TR00=4,DREQ=2,BUSY=1 };
    void put(int val) { ring[rp]=uint8_t(val); rp=(rp+1)%ring.size(); }
    int get() { int v=ring[rp]; rp=(rp+1)%ring.size(); return v; }
    bool find() {
        if(sec<1 || sec>d.sectors || trk<0 || trk>=d.tracks) return false;
        position=(int64_t(trk)*d.sectors+sec-1)*d.bps; return true;
    }
    void error(int code) { res=code; stat=IDLE; ir=true; }
    bool readSector() {
        if(!f.read(position,bb.data(),d.bps)) { error(NFND); return false; }
        if(mode&8) for(int i=0;i<d.bps;++i) put(bb[i]);
        return true;
    }
    void state(char t) {
        if(stat!=IDLE && ++timeout>148) { stat=IDLE; res=0; }
        if((stat==READ && t=='R' && cnt>=d.bps) || (stat==WRITE && t=='W' && cnt>=d.bps)) {
            bool writing=stat==WRITE;
            if(writing && !f.write(position,bb.data(),d.bps)) { error(f.readonly?WINH:NFND); return; }
            cnt=0;
            if(ctl&16) {
                ++sec;
                if(find()) { timeout=0; if(!writing) readSector(); }
                else { stat=IDLE; res=0; ir=true; }
            } else { stat=IDLE; res=0; ir=true; }
        }
        if(stat==HEAD && t=='R' && cnt>=6) { stat=IDLE; res=0; ir=true; }
        if(stat==SEEK && t=='S' && ++cnt>=8) { stat=IDLE; res=trk==0?TR00:0; ir=true; }
    }
    void selDrive(int num) {
        if(open==num) return;
        f.close(); open=0;
        if(num && f.open(cfg.drives[num-1])) { open=num; d=cfg.geometry[num-1]; }
    }
public:
    Wd(Config& c,Clock& clock):cfg(c),clk(clock) { clk.listener=[this](){ clockTimeout(); }; reset(); }
    void reset() { trk=0; sec=1; ctl=0; res=TR00; dat=cnt=timeout=stat=0; ir=false; }
    int isInt() const { return ir?1:0; }
    int isInUse() const { return open; }
    bool insertImage(int index,const std::string& path) { if(index<1 || index>4 || index==open) return false; DiskFile probe; if(!probe.open(path)) return false; cfg.drives[index-1]=path; return true; }
    bool ejectImage(int index) { if(index<1 || index>4 || index==open) return false; cfg.drives[index-1].clear(); return true; }
    void setTrk(int val) { trk=val; }
    int getTrk() const { return trk; }
    void setSec(int val) { sec=val; }
    int getSec() const { return sec; }
    int getRes() { ir=false; state('S'); return res; }
    void setDat(int val) { dat=val; if(stat==WRITE && cnt<d.bps) bb[cnt]=uint8_t(val); ++cnt; state('W'); }
    int getDat() { dat=bb[size_t(cnt)%bb.size()]; ++cnt; state('R'); return dat; }
    void setBuf(int val) { if(mode&8) put(val); }
    int getBuf() { return (mode&8)?get():0; }
    void setMode(int val) { mode=val; if(!(mode&1)) rp=0; }
    void set3212(int val) {
        r3212=val; if(!(val&4)) reset();
        switch(val&0x6a) { case 0x40:selDrive(1);break; case 0x20:selDrive(2);break;case 8:selDrive(3);break;case 2:selDrive(4);break;default:selDrive(0); }
    }
    void setCmd(int val) {
        ctl=val; ir=false; timeout=0;
        int cmd=val&0xf0;
        if(cmd==0xd0) { clockTimeout(); return; }
        if(!open) { res=NRDY | ((cmd==0 || cmd==16) && !trk?TR00:0); return; }
        if(cmd==0 || cmd==16) { trk=cmd==0?0:dat; cnt=0; res=BUSY;stat=SEEK; if(mode&8) clk.setTimeout(50); return; }
        if(!find()) { error(NFND); return; }
        cnt=0;
        if(cmd==0x80 || cmd==0x90) {
            if(!readSector()) return;
            stat=READ; res=BUSY|DREQ;
            if(mode&8) { cnt=d.bps; clk.setTimeout(50); }
        } else if(cmd==0xa0 || cmd==0xb0) {
            if(f.readonly) { error(WINH); return; }
            stat=WRITE; res=BUSY|DREQ;
            if(mode&8) {
                for(int i=0;i<d.bps;++i) bb[i]=uint8_t(get());
                if(!f.write(position,bb.data(),d.bps)) { error(NFND); return; }
                cnt=d.bps; clk.setTimeout(50);
            }
        } else if(cmd==0xc0) {
            bb[0]=uint8_t(trk); bb[1]=0; bb[2]=uint8_t(sec); bb[3]=0; bb[4]=0x81; bb[5]=0x82;
            if(++sec>d.sectors) sec=1;
            stat=HEAD; res=BUSY|DREQ;
            if(mode&8) { for(int i=0;i<6;++i) put(bb[i]); cnt=6; clk.setTimeout(50); }
        }
    }
    void clockTimeout() { res=(stat==SEEK && trk==0)?TR00:0; ir=true; stat=IDLE; }
};
