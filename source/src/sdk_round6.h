// MIT. In-memory name/splash helper tests; no setters, display changes or storage writes.
#pragma once
#include <resona/platform.h>
#include <resona/splash.h>
#include <resona/libc.h>
namespace round6 {
static const resona_platform *P;
static unsigned checks,bad;
static void test(const char*label,bool ok){checks++;if(!ok){bad++;char s[128];P->format(s,sizeof s,"R6 FAIL check=%u label=%s",checks,label);P->log(s);}}
static int finish(const char*label){char s[128];P->format(s,sizeof s,"R6 %s SUMMARY checks=%u bad=%u rc=%d",label,checks,bad,bad?-2:0);P->log(s);return bad?-2:0;}
static int names(const resona_platform*p){
 P=p;checks=bad=0;char s[52]={};
 test("usb ASCII",resona_usb_name_valid("Resona Box"));test("usb Unicode",resona_usb_name_valid("音量"));
 test("usb supplementary",resona_usb_name_valid("\xf0\x9f\x8e\xb5"));
 memset(s,'A',47);s[47]=0;test("USB47",resona_usb_name_valid(s));s[47]='A';s[48]=0;test("USB48",!resona_usb_name_valid(s));
 const char*invalid[]={"","a\nb","a\x7f","\xc2\x85","\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\x80","A\xe9\x9f"};
 for(const char*x:invalid){test("USB invalid",!resona_usb_name_valid(x));test("BT audio invalid",!resona_bluetooth_name_valid(x,31));test("BT MIDI invalid",!resona_bluetooth_name_valid(x,28));}
 for(unsigned which=0;which<3;which++){
  unsigned limit=which==0?47:which==1?31:28;
  memset(s,'A',limit-2);s[limit-2]=(char)0xc3;s[limit-1]=(char)0xa9;s[limit]=0;
  test("multibyte exact byte cap",which==0?resona_usb_name_valid(s):resona_bluetooth_name_valid(s,limit));
  memset(s,'A',limit-1);s[limit-1]=(char)0xc3;s[limit]=(char)0xa9;s[limit+1]=0;
  test("multibyte exceeds byte cap",!(which==0?resona_usb_name_valid(s):resona_bluetooth_name_valid(s,limit)));
 }
 resona_usb_names usb={"Box","Audio","MIDI"};test("USB aggregate valid",resona_usb_names_valid(&usb));usb.midi[0]=0;test("USB one invalid field",!resona_usb_names_valid(&usb));test("USB NULL aggregate",!resona_usb_names_valid(nullptr));
 resona_bluetooth_names bt={"Audio","MIDI"};test("BT aggregate valid",resona_bluetooth_names_valid(&bt));memset(bt.midi,'A',29);bt.midi[29]=0;test("BT MIDI29",!resona_bluetooth_names_valid(&bt));test("BT NULL aggregate",!resona_bluetooth_names_valid(nullptr));
 return finish("NAMES");
}
static void repair_crc(uint8_t*d,unsigned n){resona_splash_put(d+8,resona_splash_crc32(d+32,n-32));}
static int splash(const resona_platform*p){
 P=p;checks=bad=0;resona_splash_builder b{};uint8_t small[128],original[48];
 test("CRC32 known vector",resona_splash_crc32("123456789",9)==0xcbf43926U);
 memset(small,0xa5,sizeof small);test("begin31",resona_splash_begin(&b,small,31,1)==RESONA_ERR_ARG&&small[0]==0xa5);
 test("begin32",resona_splash_begin(&b,small,32,0)==0);test("no room frame",resona_splash_frame(&b,16)==RESONA_ERR_NO_MEMORY&&b.used==32&&!b.frames);test("empty finish",resona_splash_finish(&b)==RESONA_ERR_STATE);
 test("begin44",resona_splash_begin(&b,small,44,100)==0);test("frame16",resona_splash_frame(&b,16)==0);test("zero run",resona_splash_run(&b,0,0)==RESONA_ERR_ARG);test("oversize run",resona_splash_run(&b,55041,0)==RESONA_ERR_ARG);
 test("solid run",resona_splash_run(&b,55040,0x1234)==0);int n=resona_splash_finish(&b);test("exact44 valid",n==44&&resona_splash_valid(small,44));test("extra pixel refused",resona_splash_run(&b,1,0)==RESONA_ERR_ARG&&resona_splash_finish(&b)==44);
 memcpy(original,small,44);
 for(unsigned i=0;i<15;i++){
  memcpy(small,original,44);unsigned size=44;
  switch(i){
  case 0:small[0]='X';break;case 1:resona_splash_put(small+4,45);break;
  case 2:small[8]^=1;break;case 3:resona_splash_put(small+12,0x80000000U);break;
  case 4:resona_splash_put(small+16,319);break;case 5:resona_splash_put(small+20,171);break;
  case 6:resona_splash_put(small+24,0);break;case 7:resona_splash_put(small+24,129);break;
  case 8:resona_splash_put(small+28,1);break;
  case 9:resona_splash_put(small+32,15);repair_crc(small,size);break;
  case 10:resona_splash_put(small+32,60001);repair_crc(small,size);break;
  case 11:resona_splash_put(small+36,0);repair_crc(small,size);break;
  case 12:small[40]=small[41]=0;repair_crc(small,size);break;
  case 13:small[40]=0xff;small[41]=0xff;repair_crc(small,size);break;
  case 14:size=48;memset(small+44,0,4);resona_splash_put(small+4,size);repair_crc(small,size);break;
  }
  test("malformed RSP rejected",!resona_splash_valid(small,size));
 }
 test("incomplete begin",resona_splash_begin(&b,small,sizeof small,100)==0&&resona_splash_frame(&b,16)==0&&resona_splash_run(&b,55039,0)==0);
 test("incomplete frame refused",resona_splash_frame(&b,16)==RESONA_ERR_STATE&&b.frames==1&&b.pixels==55039);
 test("complete partial frame",resona_splash_run(&b,1,0)==0);n=resona_splash_finish(&b);test("two runs valid",n==48&&resona_splash_valid(small,n));
 test("hold limits",resona_splash_begin(&b,small,sizeof small,100)==0&&resona_splash_frame(&b,15)==RESONA_ERR_ARG&&resona_splash_frame(&b,60001)==RESONA_ERR_ARG&&resona_splash_frame(&b,60000)==0);
 test("duration overflow",resona_splash_begin(&b,small,sizeof small,0x80000000U)==RESONA_ERR_ARG);
 const unsigned capacity=RESONA_SPLASH_MAX_BYTES+1;uint8_t*arena=(uint8_t*)P->alloc(capacity+32,16);
 if(!arena){P->log("R6 SPLASH allocation24KiB refused");return RESONA_ERR_NO_MEMORY;}
 memset(arena,0xa5,capacity+32);uint8_t*data=arena+16;
 test("capacity clamp",resona_splash_begin(&b,data,capacity,0x7fffffffU)==0&&b.capacity==RESONA_SPLASH_MAX_BYTES);
 bool frames=true;for(unsigned i=0;i<128;i++)if(resona_splash_frame(&b,16)||resona_splash_run(&b,55040,(uint16_t)i))frames=false;
 test("128 frames",frames&&b.frames==128);n=resona_splash_finish(&b);test("128-frame valid",n==1568&&resona_splash_valid(data,n));
 unsigned used=b.used;test("129th frame refused",resona_splash_frame(&b,16)==RESONA_ERR_FULL&&b.frames==128&&b.used==used);
 test("finish idempotent",resona_splash_finish(&b)==n&&resona_splash_valid(data,n));
 bool guards=true;for(unsigned i=0;i<16;i++)if(arena[i]!=0xa5||data[capacity+i]!=0xa5)guards=false;test("arena guards",guards);
 test("oversized validator",!resona_splash_valid(data,capacity));P->free(arena);
 return finish("SPLASH");
}
}
