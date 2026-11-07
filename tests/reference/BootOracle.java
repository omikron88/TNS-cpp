// Headless boot oracle for original Java TNS. See tools/compare_java.py.
import machine.*;
import java.lang.reflect.*;
import java.awt.image.BufferedImage;
import z80core.*;
public class BootOracle {
 static Object field(Object o,String n)throws Exception { Field f=o.getClass().getDeclaredField(n); f.setAccessible(true);return f.get(o); }
 static int hash(Object array)throws Exception { int h=0x811c9dc5; for(byte[] a:(byte[][])array)for(byte b:a)h=(h^(b&255))*16777619; return h; }
 public static void main(String[] a)throws Exception {
  Tns m=new Tns(); m.setScreen(new gui.Screen());
  Field pause=Tns.class.getDeclaredField("paused");pause.setAccessible(true);pause.set(m,false);
  if(a.length>0 && a[0].equals("itk")){m.getConfig().videoitk=true;m.Reset(true);}
  m.setLed1(new javax.swing.JLabel());m.setLed2(new javax.swing.JLabel());m.setLed3(new javax.swing.JLabel());m.setLed4(new javax.swing.JLabel());
  if(a.length>1) m.getWDC().insertImage(1,a[1]);
  Z80 cpu=(Z80)field(m,"cpu");Memory mem=(Memory)field(m,"mem");
  for(int n=0;n<(a.length>1?500:100);n++) {
   if(a.length>1 && (n==100 || n==125 || n==150)) {
    Object k=field(m,"key");Field value=k.getClass().getDeclaredField("key");value.setAccessible(true);value.set(k,n==100?70:(n==125?48:13));
    Field pressed=k.getClass().getDeclaredField("pressed");pressed.setAccessible(true);pressed.set(k,true);
   }
   m.ms20(); int h=0x811c9dc5;
   for(byte[][] pages:new byte[][][]{(byte[][])field(mem,"Ram"),(byte[][])field(mem,"VRam")})for(byte[] p:pages)for(byte b:p)h=(h^(b&255))*16777619;
   int img=0x811c9dc5; BufferedImage im=m.getImage();for(int y=0;y<256;y++)for(int x=0;x<640;x++)img=(img^im.getRGB(x,y))*16777619;
   System.out.printf("TRACE %d %d %d %d %d %d %d %d %d %d\n",n,cpu.getRegPC(),cpu.getRegAF(),cpu.getRegBC(),cpu.getRegDE(),cpu.getRegHL(),cpu.getRegSP(),((Clock)field(m,"clk")).getTstates(),h,img);
  }
  System.exit(0);
 }
}
