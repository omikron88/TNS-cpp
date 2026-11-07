// Differential oracle for the original Java CPU. GPL-3.0; see LICENSE.
import machine.Clock;
import z80core.*;
public class CpuOracle implements MemIoOps, NotifyOps {
    Clock clock=new Clock();byte[] mem=new byte[65536];int io=0,state=0;
    int next() { state=state*1664525+1013904223;return state>>>16; }
    public int fetchOpcode(int a){clock.addTstates(4);return mem[a&65535]&255;}
    public int peek8(int a){clock.addTstates(3);return mem[a&65535]&255;}
    public void poke8(int a,int v){clock.addTstates(3);mem[a&65535]=(byte)v;}
    public int peek16(int a){int l=peek8(a);return l|(peek8((a+1)&65535)<<8);}
    public void poke16(int a,int v){poke8(a,v);poke8((a+1)&65535,v>>>8);}
    public int intAck(int a){clock.addTstates(6);return(mem[a&65535]&255)|((mem[(a+1)&65535]&255)<<8);}
    public int inPort(int p){clock.addTstates(4);return(p*17+29)&255;}
    public void outPort(int p,int v){clock.addTstates(4);io=(io*31)^p^(v<<16);}
    public void contendedStates(int a,long t){}
    public int atAddress(int a,int op){return op;}
    public void execDone(){}
    void run(int group,int op,int seed) {
        state=seed+group*65537+op*257;for(int n=0;n<65536;++n)mem[n]=(byte)next();
        Z80 c=new Z80(clock,this,this);
        c.setRegAF(next());c.setRegBC(next());c.setRegDE(next());c.setRegHL(next());
        c.setRegAFx(next());c.setRegBCx(next());c.setRegDEx(next());c.setRegHLx(next());
        c.setRegIX(next());c.setRegIY(next());c.setRegSP(next());c.setRegI(next());c.setRegR(0x8d);c.setRegPC(0x4000);
        if(group==0)mem[0x4000]=(byte)op;
        else if(group<=4){int[] p={0,0xcb,0xed,0xdd,0xfd};mem[0x4000]=(byte)p[group];mem[0x4001]=(byte)op;}
        else {mem[0x4000]=(byte)(group==5?0xdd:0xfd);mem[0x4001]=(byte)0xcb;mem[0x4003]=(byte)op;}
        c.execute(1);
        int hash=0x811c9dc5;for(byte b:mem)hash=(hash^(b&255))*16777619;
        System.out.printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",group,op,seed,c.getRegPC(),c.getRegAF(),c.getRegBC(),c.getRegDE(),c.getRegHL(),c.getRegAFx(),c.getRegBCx(),c.getRegDEx(),c.getRegHLx(),c.getRegIX(),c.getRegIY(),c.getRegSP(),((c.getRegI()<<8)|c.getRegR()),c.getMemPtr(),c.isHalted()?1:0,c.isIFF1()?1:0,c.isPendingEI()?1:0,clock.getTstates(),hash,io);
    }
    public static void main(String[] args){for(int g=0;g<7;++g)for(int op=0;op<256;++op)for(int seed=1;seed<=2;++seed)new CpuOracle().run(g,op,seed);}
}
