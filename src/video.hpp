#pragma once
#include "memory.hpp"
#include <array>
struct Image {
std::array<uint32_t,640*256> pixels{};
void setRGB(int x,int y,int color) { pixels[y*640+x]=0xff000000u|uint32_t(color); }
};
class Grafik {
public:

    int ofsx = 64;
    int ofsy = 0;

    int palc[16] = {
        0x000000, 0x000000, 0x0000cc, 0x0000ff, 0xcc0000, 0xff0000, 0xcc00cc, 0xff00ff,
        0x00cc00, 0x00ff00, 0x00cccc, 0x00ffff, 0xcccc00, 0xffff00, 0xcccccc, 0xffffff,
    };

    int palb[16] = {
        0x000000, 0x101010, 0x202020, 0x303030, 0x404040, 0x505050, 0x606060, 0x707070,
        0x808080, 0x909090, 0xa0a0a0, 0xb0b0b0, 0xc0c0c0, 0xd0d0d0, 0xe0e0e0, 0xf0f0f0,
    };

    Memory& m;
    Image& i;
    Config& cfg;

    int *pal;

    int mode;
    bool ir;

    Grafik(Memory& mem, Image& img, Config& c) : m(mem), i(img), cfg(c), mode(0), ir(false) { reSetPalette(); }

    void reSetPalette() {
        pal = cfg.videobw ? palb : palc;
    }

    void reset() {
        setMode(0);
        ir = false;
    }

    void vSync() {
        ir = true;
    }

    void setMode(int mod) {
        ir = false;
        mode = mod;
    }

    int getMode() {
        ir = false;
        return 0;
    }

    int isInt() {
        bool tmp = ir;
        ir = false;
        return tmp ? 1:0;
    }

    void paint() {
        if ((mode&0x04)!=0) {
            g0();
        }
        else {
            if ((mode&0x02)==0) {
                g1();
            }
            else {
                g2();
            }
        }
    }

    void g0() {
        int c,d,ad,adr;
        int ink, pap;
        int b;

        adr = ((mode&0x08)!=0) ? 0x7580 : 0xf580;
        for (int y=0; y<21; y++) {
            for (int x=0; x<64; x++) {
                ad = m.readVram(adr++) & 0xff;
                ink = pal[(ad & 0xf0) >> 4];
                pap = pal[ad & 0x0f];
                ad = ((m.readVram(adr++) & 0xff) << 4)+12;
                d = (y * 12) + ofsy;
                for (int a=0; a<12; a++) {
                    c = (x << 3) + ofsx;
                    b = m.readChar(--ad);
                    i.setRGB(c++, d, (b & 0x80) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x40) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x20) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x10) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x08) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x04) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x02) == 0 ? pap : ink);
                    i.setRGB(c++, d, (b & 0x01) == 0 ? pap : ink);
                    d++;
                }
            }
        }
        for (int y=(252+ofsy); y<(256+ofsy); y++) {
            for (int x=(0+ofsx); x<(512+ofsx); x++) {
                i.setRGB(x, y, 0);
                }
            }
    }

void g1() {
         int c,d,ad,bd,adr;
         int ink, pap;

         adr = ((mode&0x08)!=0) ? 0x0000 : 0x8000;
         for (int y=0; y<256; y++) {
            d = y + ofsy;
            for (int x=0; x<64; x++) {
                ad = m.readVram(adr++) & 0xff;
                bd = (m.readVram(adr++) & 0xff);
                ink = pal[(ad >> 4)& 0x0f];
                pap = pal[ad & 0x0f];
                c = x * 8 + ofsx;
                i.setRGB(c++, d, (bd & 0x80) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x40) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x20) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x10) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x08) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x04) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x02) == 0 ? pap : ink);
                i.setRGB(c++, d, (bd & 0x01) == 0 ? pap : ink);
            }
         }
        }

void g2() {
         int c,d,ad,bd,adr;

         adr = ((mode&0x08)!=0) ? 0x0000 : 0x8000;
         for (int y=0; y<256; y++) {
            d = y + ofsy;
            for (int x=0; x<128; x++) {
                ad = m.readVram(adr++) & 0xff;
                bd = pal[ad & 0x0f];
                ad = pal[(ad >> 4)& 0x0f];
                c = x * 4 + ofsx;
                i.setRGB(c++, d, bd);i.setRGB(c++, d, bd);
                i.setRGB(c++, d, ad);i.setRGB(c++, d, ad);
            }
         }
        }
};

class Itk {
    Memory& m; Image& i; Config& cfg;
    bool ir=false;
public:
    Itk(Memory& mem,Image& img,Config& c):m(mem),i(img),cfg(c) {}
    void reset() { ir=false; }
    void vSync() { ir=true; }
    int getMode() { ir=false; return 0; }
    void setMode(int) { ir=false; }
    int isInt() { bool v=ir; ir=false; return v?1:0; }
    void paint() {
        int adr=0xfc00;
        for(int y=0;y<16;++y) for(int x=0;x<64;++x) {
            int ch=m.readByte(adr++);
            for(int a=0;a<12;++a) {
                int b=m.readIChar(ch*12+a);
                for(int bit=0;bit<6;++bit) i.setRGB(64+x*6+bit,y*12+a,(b&(32>>bit))?(cfg.videobw?0xffffff:0x00ff00):0);
            }
        }
    }
};
