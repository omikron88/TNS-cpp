#pragma once
#include "z80.hpp"
#include "video.hpp"
#include "keyboard.hpp"
#include "disks.hpp"
class Tns : public MemIoOps, public NotifyOps {
public:
    Config cfg;
    Image image;
    Memory mem;
    Clock clk;
    Z80 cpu;
    Grafik grf;
    Itk itk;
    Keyboard key;
    Wd wdc;
    Wap wap;
    bool boot=true,fdcsync=false,runap=false,blank=false;
    int ap=0,pfr=0,pfw=0,mapp=0;
    std::array<bool,128> mask{};
    int map[2][8]{};
    std::array<bool,4> mape{},pfle{};
    bool mapa=false,pflp=false;
    explicit Tns(const Config& config=Config(),const std::string& romdir="") :
        cfg(config),mem(cfg,romdir),cpu(clk,*this,*this),grf(mem,image,cfg),itk(mem,image,cfg),wdc(cfg,clk),wap(cfg) { Reset(true); }
    Tns(const Tns&)=delete;
    Tns& operator=(const Tns&)=delete;
    void Reset(bool dirty) {
        boot=true;fdcsync=false;runap=false;ap=0;pfr=pfw=mapp=0;mapa=false;
        mape.fill(false);pfle.fill(false);
        mem.reset(dirty);mem.bootRom(true);clk.reset();cpu.reset();grf.reset();itk.reset();key.reset();wdc.reset();wap.reset();
        image.pixels.fill(0xff000000u);
    }
    void Nmi() { cpu.setNMI(true);cpu.execute(clk.getTstates()+8);cpu.setNMI(false); }
    int isBlank() const { return blank?2:0; }
    void frame() {
        if(cfg.videoitk) { itk.vSync();itk.paint(); } else { grf.vSync();grf.paint(); }
        blank=true;cpu.execute(clk.getTstates()+1000);
        blank=false;cpu.execute(clk.getTstates()+39000);
    }
    void delayMAP() {
        mape[3] = mape[2];
        mape[2] = mape[1];
        mape[1] = mape[0];

        pfle[3] = pfle[2];
        pfle[2] = pfle[1];
        pfle[1] = pfle[0];

        pflp = !pflp;
    }

    int fetchOpcode(int address) {
        clk.addTstates(4);

        if (runap) {
            ap = ((ap + 2) & 0xfe) | 1;
            if ((testINT(ap) & 1) !=0 )  {
                if (mask[ap>>1]) {
                    runap = false;
                    cpu.setINTLine(true);

                }
            }
        }

        int addr = address;
        if (mape[3]) {
            addr &= 0x1fff;
            addr |= map[mapp][address>>13];
        }

        int opcode = mem.readByte(addr) & 0xff;

        if (fdcsync==true) {
            fdcsync = false;
            opcode = 0x00;
        }

        delayMAP();
        return opcode;
    }

    int peek8(int address) {
        clk.addTstates(3);

        int addr = address;
        if (!pfle[3]) {
            if (mape[3]) {
                addr &= 0x1fff;
                addr |= map[mapp][address>>13];
            }
        }
        else {
            if (!pflp) addr |= pfw; else addr |= pfr;
        }

        int value = mem.readByte(addr) & 0xff;

        delayMAP();
        return value;
    }

    void poke8(int address, int value) {

        clk.addTstates(3);

        int addr = address;
        if (!pfle[3]) {
            if (mape[3]) {
                addr &= 0x1fff;
                addr |= map[mapp][address>>13];
            }
        }
        else {
            if (!pflp) addr |= pfw; else addr |= pfr;
        }

        mem.writeByte(addr, (uint8_t) value);
        delayMAP();
    }

    int peek16(int address) {
        int lsb = peek8(address) & 0xff;
        address = (address+1) & 0xffff;
        return ((peek8(address) << 8) & 0xff00 | lsb);
    }

    void poke16(int address, int word) {
        poke8(address, (uint8_t) word);
        address = (address+1) & 0xffff;
        poke8(address, (uint8_t) (word >> 8));
    }

    int intAck(int address) {
        clk.addTstates(6);
        if (mapa==false) {mape.fill(false);}
        pfle.fill(false);
        cpu.setINTLine(false);
        address |= (ap & 0x00fe);

        int lsb = mem.readByte(address) & 0xff;
        address = (address+1) & 0xffff;
        return ((mem.readByte(address) << 8) & 0xff00 | lsb);
    }

    int testINT(int port) {
        switch(port & 0xff) {
            case 0x2d: return (!cfg.videoitk) ? grf.isInt() : 0;
            case 0x2f: return (!cfg.videoitk) ? key.isKey() : 0;
            case 0x59: return wap.isWInt();
            case 0x5b: return wap.isRInt();
            case 0x61: return wdc.isInt();
            case 0xb1: return (cfg.videoitk) ? itk.isInt() : 0;
            case 0xb9: return (cfg.videoitk) ? key.isKey() : 0;
        }
        return 0;
    }

    int inPort(int port) {
        clk.addTstates(4);
        int tmp = 0x00;

        pfle.fill(false);
        delayMAP();

        switch(port & 0xff) {
            case 0x2c:
                return (!cfg.videoitk) ? grf.getMode() : 0;
            case 0x2d:
                return (!cfg.videoitk) ? grf.isInt() : 0;
            case 0x2e:
                return (!cfg.videoitk) ? key.getKey() : 0;
            case 0x2f:
                return (!cfg.videoitk) ? key.isKey() : 0;
            case 0x3a:
                return 0;
            case 0x59:
                return wap.isWInt();
            case 0x5a:
                return wap.wapIn();
            case 0x5b:
                return wap.isRInt();
            case 0x5c:
                {
                    int val;
                    if ((port&0x1000)==0) {
                        val = map[0][(port&0xe000)>>13] >> 12;
                    }
                    else {
                        val = map[1][(port&0xe000)>>13] >> 12;
                    }
                    return val;
                }
            case 0x5e:
                {
                    int val = 0;
                    if (mape[0]) {val |= 0x80;}
                    if (mapa)    {val |= 0x02;}
                    if (mapp==1) {val |= 0x01;}
                    return val;
                }
            case 0x60:
                return wdc.getRes();
            case 0x62:
                return wdc.getTrk();
            case 0x64:
                return wdc.getSec();
            case 0x66:
                return wdc.getDat();
            case 0x68:
                return 0xff;
            case 0x6A:
                return wdc.getBuf();
            case 0x61:
                return wdc.isInt();
            case 0xb0:
                return (cfg.videoitk) ? itk.getMode() : 0;
            case 0xb1:
                return (cfg.videoitk) ? itk.isInt() : 0;
            case 0xb8:
                return (cfg.videoitk) ? key.getKey() : 0;
            case 0xb9:
                return (cfg.videoitk) ? key.isKey()|isBlank() : 0;
        }

        if ((port & 0x0001) != 0) {
            tmp |= mask[ap>>1] ? 0x08 : 0x00;
        }
        else {

        }
        return tmp;
    }

    void outPort(int port, int value) {
        clk.addTstates(4);
        value = value & 0xff;

        if (boot) {
            boot = false;
            mem.bootRom(boot);
        }

        runap = true;

        if ((port & 0x0001) != 0) {
            mask[(port&0xff)>>1] = ((value & 1) != 0);

        }

        switch(port & 0xff) {
            case 0x0e: {mape[0] = !mape[0]; break;}
            case 0x2c:
                {

                    if (!cfg.videoitk) grf.setMode(value);
                    break;
                }
            case 0x3a:
                {

                    pfr = (value & 0x0f) << 16;
                    pfw = (value & 0xf0) << 12;
                    pflp = false;
                    pfle[0] = true;
                    break;
                }
            case 0x58:
                {
                    wap.wapOut(value);
                    break;
                }
            case 0x5a:
                {

                    break;
                }
            case 0x5c:
                {
                    if ((port&0x1000)==0) {
                        map[0][(port&0xe000)>>13] = (value&0xfe) << 12;

                    }
                    else {
                        map[1][(port&0xe000)>>13] = (value&0xfe) << 12;

                    }
                    break;
                }
            case 0x5e:
                {

                    mapp = (value&1)==0 ? 0 : 1;
                    mapa = (value&2) != 0;
                    break;
                }
            case 0x60:
                {wdc.setCmd(value); break;}
            case 0x62:
                {wdc.setTrk(value); break;}
            case 0x64:
                {wdc.setSec(value); break;}
            case 0x66:
                {wdc.setDat(value); break;}
            case 0x68:
                {wdc.set3212(value); break;}
            case 0x6A:
                {wdc.setBuf(value); break;}
            case 0x6C:
                {fdcsync = true; break;}
            case 0x6E:
                {wdc.setMode(value); break;}
            case 0xB0:
                {if (cfg.videoitk) {itk.setMode(value);} break;}
            default:
            {
                if ((port & 0x0001) == 0) {

                }
            }
        }

        delayMAP();
    }

    void contendedStates(int, int64_t) {

    }

};
