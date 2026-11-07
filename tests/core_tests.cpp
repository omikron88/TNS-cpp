#include "tns.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <memory>
#include <cstdio>
#define CHECK(c) do { if(!(c)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": "+#c); } while(0)
struct TestImage {
    std::string name;
    TestImage(size_t size,const std::string& suffix="") {
        name="tns-test-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+suffix+".img";
        CHECK(!std::ifstream(name));std::ofstream f(name,std::ios::binary);
        for(size_t i=0;i<size;++i) {f.put(char(i&255));}
        CHECK(bool(f));
    }
    ~TestImage(){removeFile(name);}
};
static void boot(const std::string& dir,bool itk,bool demo=false) {
    TestImage disk(0);
    if(demo) {std::ifstream source(dir+"/../media/tnsdemo.8sd",std::ios::binary);CHECK(bool(source));std::ofstream target(disk.name,std::ios::binary);target<<source.rdbuf();CHECK(bool(target));}
    Config c;c.videoitk=itk;if(demo)c.drives[0]=disk.name;std::unique_ptr<Tns> machine(new Tns(c));Tns& m=*machine;
    std::ifstream ref(dir+(demo?"/java-demo.trace":(itk?"/java-itk.trace":"/java-gc.trace")));CHECK(bool(ref));
    for(int n=0;n<(demo?500:100);++n) {
        if(demo && n==100)m.key.press('F',true,false,false,false,false,false,false);
        if(demo && n==125)m.key.press(96,false,false,false,false,true,false,false);
        if(demo && n==150)m.key.press(10,false,false,false,false,false,false,false);
        m.frame();uint32_t h=2166136261u;for(auto p:m.image.pixels)h=(h^p)*16777619u;
        std::ostringstream actual;
        actual<<"TRACE "<<n<<' '<<m.cpu.getRegPC()<<' '<<m.cpu.getRegAF()<<' '<<m.cpu.getRegBC()<<' '<<m.cpu.getRegDE()<<' '<<m.cpu.getRegHL()<<' '<<m.cpu.getRegSP()<<' '<<m.clk.getTstates()<<' '<<int32_t(m.mem.checksum())<<' '<<int32_t(h);
        std::string expected;CHECK(bool(std::getline(ref,expected)));CHECK(expected==actual.str());
    }
    std::string extra;CHECK(!std::getline(ref,extra));
}
static void files() {
    TestImage temp(0);std::string path=temp.name+u8"-příliš.config";
    Config c;c.hdd=u8"příliš/demo.hdd";c.drives[0]=u8"žluťoučký.img";
    CHECK(c.save(path));Config d;d.load(path);CHECK(d.hdd==c.hdd);CHECK(d.drives[0]==c.drives[0]);removeFile(path);
    FILE* f=openFile(path,"wb");CHECK(f!=nullptr);uint8_t buf[128]{};CHECK(std::fwrite(buf,1,sizeof buf,f)==128);std::fclose(f);
    DiskFile disk;CHECK(disk.open(path));CHECK(disk.read(0,buf,128));disk.close();removeFile(path);
}
static void memory() {
    Config c;std::unique_ptr<Memory> mem(new Memory(c));
    CHECK(mem->readByte(0)==0);CHECK(mem->readByte(127)==0);CHECK(mem->readByte(128)==255);
    mem->writeByte(0,123);mem->bootRom(true);CHECK(mem->readByte(0)==rom_bootGC[0]);
    mem->writeByte(0,45);mem->bootRom(false);CHECK(mem->readByte(0)==45);
    CHECK(mem->readByte(0x80000)==255);mem->writeByte(0x80000,42);CHECK(mem->readByte(0x80000)==255);
    mem->writeByte(0xe0000,77);CHECK(mem->readVram(0)==77);
    c.videoitk=true;mem->reset(false);CHECK(mem->readByte(0xe0000)==255);
    c.ramsize=1024;mem->reset(false);mem->writeByte(0xfffff,92);CHECK(mem->readByte(0xfffff)==92);
}
static void mapping() {
    std::unique_ptr<Tns> machine(new Tns);Tns& m=*machine;
    m.outPort(0x5c,2); // map virtual page 0 to physical page 1
    m.mem.writeByte(8192,0x5a);m.mem.writeByte(0,0xa5);
    m.outPort(0x0e,0);CHECK(!m.mape[3]);m.peek8(0);m.peek8(0);CHECK(m.mape[3]);CHECK(m.peek8(0)==0x5a);
    m.outPort(0x3a,0x11);m.peek8(0);m.peek8(0);m.poke8(0,77);CHECK(m.mem.readByte(0x10000)==77);
    m.inPort(0x3a);CHECK(!m.pfle[3]);
    m.map[0][0]=8192;m.mask[0]=true;m.ap=0;m.Reset(false);CHECK(m.map[0][0]==8192);CHECK(m.mask[0]);
}
static void keyboard() {
    Keyboard k;k.press('A',false,false,false,false,false,false,false);CHECK(k.isKey()==1);CHECK(k.getKey()==97);CHECK(!k.isKey());
    k.press('A',true,false,false,false,false,false,false);CHECK(k.getKey()==65);
    k.press('A',false,true,false,false,false,false,false);CHECK(k.getKey()==1);
    k.press(112,false,false,false,true,false,false,false);CHECK(k.getKey()==128);
    k.press(112,false,false,false,true,false,false,true);CHECK(k.getKey()==138);
    k.press(10,false,false,false,false,false,false,false);CHECK(k.getKey()==13);
}
static void disks() {
    TestImage image(77*26*128);Config c;c.drives[0]=image.name;Clock clock;Wd wd(c,clock);
    wd.set3212(0x44);CHECK(wd.isInUse()==1);CHECK(!wd.ejectImage(1));wd.setTrk(0);wd.setSec(2);wd.setCmd(0x80);
    CHECK(wd.getRes()==3);for(int i=0;i<128;++i)CHECK(wd.getDat()==((128+i)&255));CHECK(wd.isInt()==1);CHECK(wd.getRes()==0);
    wd.setSec(1);wd.setCmd(0xa0);for(int i=0;i<128;++i)wd.setDat(255-i);CHECK(wd.isInt()==1);
    wd.setCmd(0x80);for(int i=0;i<128;++i)CHECK(wd.getDat()==255-i);
    wd.setMode(8);wd.setSec(2);wd.setCmd(0x80);wd.setMode(8);for(int i=0;i<128;++i)CHECK(wd.getBuf()==((128+i)&255));clock.addTstates(50);CHECK(wd.isInt());
    wd.setMode(8);for(int i=0;i<128;++i)wd.setBuf(42);wd.setMode(8);wd.setSec(1);wd.setCmd(0xa0);clock.addTstates(50);wd.setCmd(0x80);wd.setMode(8);for(int i=0;i<128;++i)CHECK(wd.getBuf()==42);
    wd.setSec(27);wd.setCmd(0x80);CHECK(wd.getRes()==16);
    wd.set3212(4);CHECK(!wd.isInUse());CHECK(wd.ejectImage(1));wd.setCmd(0x80);CHECK(wd.getRes()&128);
    c.hdd=image.name;Wap wap(c);CHECK(wap.isRInt());
    for(int v:{3,0,0,0,1}) {wap.wapOut(v);}
    for(int i=0;i<128;++i) {CHECK(wap.wapIn()==((128+i)&255));}
    wap.wapIn();
    for(int v:{4,0,0,0,2}) {wap.wapOut(v);}
    for(int i=0;i<128;++i)wap.wapOut(67);
    for(int v:{3,0,0,0,2}) {wap.wapOut(v);}
    for(int i=0;i<128;++i)CHECK(wap.wapIn()==67);
    wap.ejectImage();CHECK(!wap.isRInt());CHECK(!wap.insertImage(image.name+".missing"));
}
struct CpuBus:MemIoOps,NotifyOps {
    Clock clk;std::array<uint8_t,65536> mem{};uint32_t io=0;
    int fetchOpcode(int a) override{clk.addTstates(4);return mem[a&65535];}
    int peek8(int a) override{clk.addTstates(3);return mem[a&65535];}
    void poke8(int a,int v) override{clk.addTstates(3);mem[a&65535]=uint8_t(v);}
    int peek16(int a) override{int l=peek8(a);return l|(peek8((a+1)&65535)<<8);}
    void poke16(int a,int v) override{poke8(a,v);poke8((a+1)&65535,v>>8);}
    int intAck(int a) override{clk.addTstates(6);return mem[a&65535]|(mem[(a+1)&65535]<<8);}
    int inPort(int p) override{clk.addTstates(4);return(p*17+29)&255;}
    void outPort(int p,int v) override{clk.addTstates(4);io=(io*31)^uint32_t(p)^(uint32_t(v)<<16);}
    void contendedStates(int,int64_t) override{}
};
static void cpuReference(const std::string& dir) {
    std::ifstream ref(dir+"/java-cpu.trace"); CHECK(bool(ref));int count=0;
    for(int group=0;group<7;++group) for(int op=0;op<256;++op) for(int seed=1;seed<=2;++seed) {
        CpuBus bus;uint32_t state=seed+group*65537+op*257;
        auto next=[&]()->int {state=state*1664525u+1013904223u;return int(state>>16);};
        for(auto& b:bus.mem)b=uint8_t(next());
        Z80 c(bus.clk,bus,bus);
        c.setRegAF(next());c.setRegBC(next());c.setRegDE(next());c.setRegHL(next());
        c.setRegAFx(next());c.setRegBCx(next());c.setRegDEx(next());c.setRegHLx(next());
        c.setRegIX(next());c.setRegIY(next());c.setRegSP(next());c.setRegI(next());c.setRegR(0x8d);c.setRegPC(0x4000);
        if(group==0)bus.mem[0x4000]=uint8_t(op);
        else if(group<=4){int p[]={0,0xcb,0xed,0xdd,0xfd};bus.mem[0x4000]=uint8_t(p[group]);bus.mem[0x4001]=uint8_t(op);}
        else {bus.mem[0x4000]=uint8_t(group==5?0xdd:0xfd);bus.mem[0x4001]=0xcb;bus.mem[0x4003]=uint8_t(op);}
        c.execute(1);uint32_t hash=2166136261u;for(auto b:bus.mem)hash=(hash^b)*16777619u;
        std::ostringstream actual;
        actual<<group<<' '<<op<<' '<<seed<<' '<<c.getRegPC()<<' '<<c.getRegAF()<<' '<<c.getRegBC()<<' '<<c.getRegDE()<<' '<<c.getRegHL()<<' '<<c.getRegAFx()<<' '<<c.getRegBCx()<<' '<<c.getRegDEx()<<' '<<c.getRegHLx()<<' '<<c.getRegIX()<<' '<<c.getRegIY()<<' '<<c.getRegSP()<<' '<<((c.getRegI()<<8)|c.getRegR())<<' '<<c.getMemPtr()<<' '<<c.isHalted()<<' '<<c.isIFF1()<<' '<<c.isPendingEI()<<' '<<bus.clk.getTstates()<<' '<<int32_t(hash)<<' '<<int32_t(bus.io);
        std::string expected;CHECK(bool(std::getline(ref,expected)));
        if(actual.str()!=expected)throw std::runtime_error("CPU oracle mismatch:\nJava: "+expected+"\nC++:  "+actual.str());
        ++count;
    }
    std::string extra;CHECK(!std::getline(ref,extra));CHECK(count==3584);
}
static void cpu() {
    CpuBus bus;Z80 cpu(bus.clk,bus,bus);
    // EI must defer interrupts for one whole instruction, then push return PC.
    bus.mem[0]=0xfb;bus.mem[1]=0x00;cpu.setINTLine(true);cpu.setRegSP(0x8000);
    cpu.execute(1);CHECK(cpu.getRegPC()==1);CHECK(cpu.isPendingEI());
    cpu.execute(bus.clk.getTstates()+1);CHECK(cpu.getRegPC()==2);CHECK(!cpu.isPendingEI());
    cpu.execute(bus.clk.getTstates()+1);CHECK(cpu.getRegPC()==0x39);CHECK(bus.mem[0x7ffe]==2);CHECK(bus.mem[0x7fff]==0);
    cpu.reset();bus.clk.reset();cpu.setRegPC(0x1234);cpu.setRegSP(0x9000);cpu.setNMI(true);cpu.execute(1);CHECK(cpu.getRegPC()==0x66);CHECK(bus.mem[0x8ffe]==0x34);
}
int main(int argc,char** argv) {
    try {CHECK(argc==2);files();memory();mapping();keyboard();cpu();cpuReference(argv[1]);disks();boot(argv[1],false);boot(argv[1],true);boot(argv[1],false,true);std::cout<<"PASS: memory, mapping/PFL, keyboard, CPU interrupts and 3584 Java instruction cases, floppy/HDD read/write, 700 Java reference frames including CP/M disk boot\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
