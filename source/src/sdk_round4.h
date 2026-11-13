// MIT. Public SDK memory/string and framebuffer probes; no persistent writes.
#pragma once
#include <resona/platform.h>
#include <resona/ui.h>
#include <resona/libc.h>
namespace round4 {
static const resona_platform *P;
static uint8_t pattern(unsigned i,unsigned n){return (uint8_t)(i*37+n*11+13);}
static int memory(const resona_platform *p){
 P=p;unsigned bad=0,strings=0;uint8_t a[160],b[160];
 for(unsigned n=0;n<64;n++){
  unsigned src=16+(n*13)%32,dst=16+(n*7)%32;
  for(unsigned i=0;i<160;i++)a[i]=pattern(i,n);
  if(memmove(a+dst,a+src,n)!=a+dst)bad++;
  for(unsigned i=0;i<160;i++){uint8_t expected=pattern(i>=dst&&i<dst+n?src+i-dst:i,n);if(a[i]!=expected){bad++;break;}}
  for(unsigned i=0;i<160;i++){a[i]=pattern(i,n);b[i]=pattern(i,n+9);}
  if(memcpy(a+dst,b+src,n)!=a+dst)bad++;
  for(unsigned i=0;i<160;i++)if(a[i]!=(i>=dst&&i<dst+n?pattern(src+i-dst,n+9):pattern(i,n))||b[i]!=pattern(i,n+9)){bad++;break;}
  for(unsigned i=0;i<160;i++)a[i]=pattern(i,n);
  if(memset(a+dst,(int)(n*19-128),n)!=a+dst)bad++;
  for(unsigned i=0;i<160;i++)if(a[i]!=(i>=dst&&i<dst+n?(uint8_t)(n*19-128):pattern(i,n))){bad++;break;}
 }
 #define STRING_CHECK(test) do{strings++;if(!(test))bad++;}while(0)
 const char highA[]={(char)128,0},highB[]={(char)255,0};
 STRING_CHECK(strcmp(highA,highB)<0);STRING_CHECK(strncmp(highA,highB,1)<0);
 STRING_CHECK(strncmp("a\0z","a\0q",3)==0);STRING_CHECK(strncmp("ab","ac",0)==0);
 char out[24];memset(out,0xa5,sizeof out);STRING_CHECK(strncpy(out,"A",4)==out&&out[0]=='A'&&!out[1]&&!out[2]&&!out[3]&&(uint8_t)out[4]==0xa5);
 memset(out,0xa5,sizeof out);STRING_CHECK(strncpy(out,"ABCDE",2)==out&&out[0]=='A'&&out[1]=='B'&&(uint8_t)out[2]==0xa5);
 memset(out,0xa5,sizeof out);STRING_CHECK(strncpy(out,"A",0)==out&&(uint8_t)out[0]==0xa5);
 memset(out,0xa5,sizeof out);out[0]='a';out[1]='b';out[2]=0;
 STRING_CHECK(strncat(out,"WXYZ",2)==out&&!strcmp(out,"abWX")&&(uint8_t)out[5]==0xa5);
 STRING_CHECK(strncat(out,"Q",0)==out&&!strcmp(out,"abWX")&&(uint8_t)out[5]==0xa5);
 char text[96];for(unsigned n=0;n<64;n++){for(unsigned i=0;i<n;i++)text[1+i]=(char)('A'+i%26);text[1+n]=0;STRING_CHECK(strlen(text+1)==n);}
 #undef STRING_CHECK
 char line[128];P->format(line,sizeof line,"R4MEM SUMMARY move=64 copy=64 fill=64 strings=%u bad=%u rc=%d",strings,bad,bad?-2:0);P->log(line);return bad?-2:0;
}
static unsigned active,index,bad,mode;
static uint32_t hashes[18];
static uint16_t fontReference[18*7];
static uint32_t previousPoints[16],previousCount;
static int ui_begin(const resona_platform *p,unsigned selected=0){P=p;index=bad=0;mode=selected;active=1;memset(hashes,0,sizeof hashes);previousCount=0;return 0;}
static bool frame(const resona_platform *p){
 if(!active)return false;P=p;char line[128];uint16_t *pixels=nullptr;uint32_t stride=0;resona_ui *u=P->screen_begin(&pixels,&stride);
 if(!u||!pixels||stride<320||u->width!=320||u->height!=172||!resona_ui_has_v1(u)){P->log("R4UI missing screen contract rc=-2");active=0;return true;}
 const uint16_t color=0xffe0;int expected=-1;P->format(line,sizeof line,"R4UI BEGIN case=%u",index);P->log(line);u->clear(u->ctx,0);
 if(mode==1){
  unsigned dots=0,sweep=0,show=0;
  if(index==0)u->text(u->ctx,10,10,1,color,"\xe9\x9f\xb3");
  else if(index==1)u->text(u->ctx,10,10,1,color,"???");
  else{
   switch(index){
   case 2:dots=8;sweep=360;show=7;break;
   case 3:dots=8;sweep=360;show=8;break;
   case 4:dots=8;sweep=315;show=8;break;
   case 5:dots=9;sweep=360;show=8;break;
   case 6:dots=9;sweep=360;show=9;break;
   case 7:dots=2;sweep=360;show=2;break;
   case 8:dots=2;sweep=180;show=2;break;
   }
   u->dots(u->ctx,40,40,15,15,0,(int32_t)sweep,dots,show,1,color);
  }
  unsigned count=0,diff=0;uint32_t points[16]={};
  for(unsigned y=0;y<172;y++)for(unsigned x=0;x<320;x++)if(pixels[y*stride+x]==color){if(count<16)points[count]=y*320+x;count++;}
  if(index==0)for(unsigned y=0;y<7;y++)for(unsigned x=0;x<18;x++)fontReference[y*18+x]=pixels[(y+10)*stride+x+10];
  if(index==1)for(unsigned y=0;y<7;y++)for(unsigned x=0;x<18;x++)if(fontReference[y*18+x]!=pixels[(y+10)*stride+x+10])diff++;
  if(index==3||index==6){diff=count!=previousCount;for(unsigned i=0;i<count&&i<16;i++)if(points[i]!=previousPoints[i])diff++;}
  if(index==2||index==5){previousCount=count;memcpy(previousPoints,points,sizeof points);}
  P->format(line,sizeof line,"R4FOCUS case=%u count=%u dots=%u sweep=%u show=%u pixel_diff=%u",index,count,dots,sweep,show,diff);P->log(line);
  P->screen_present(0,0,320,172);
  if(++index==9){P->log("R4FOCUS COMPLETE cases=9");active=0;}
  return true;
 }
 const int16_t xy[]={10,10,15,10,15,15,10,15};
 switch(index){
 case 0:expected=0;break;
 case 1:u->rect(u->ctx,5,7,11,13,color);expected=143;break;
 case 2:u->rect(u->ctx,-3,-2,8,6,color);expected=20;break;
 case 3:u->rect(u->ctx,318,170,8,8,color);expected=4;break;
 case 4:u->rect(u->ctx,8,8,0,0,color);expected=0;break;
 case 5:u->line(u->ctx,10,10,15,10,color);expected=6;break;
 case 6:u->line(u->ctx,10,10,15,15,color);expected=6;break;
 case 7:u->dotline(u->ctx,10,10,20,10,3,color);expected=4;break;
 case 8:u->poly(u->ctx,xy,4,color);expected=20;break;
 case 9:u->poly_fill(u->ctx,xy,4,color);break;
 case 10:u->ellipse(u->ctx,15,15,3,2,color);break;
 case 11:u->ellipse_fill(u->ctx,15,15,3,2,color);break;
 case 12:u->dots(u->ctx,40,40,15,15,0,360,8,8,1,color);break;
 case 13:u->text(u->ctx,10,10,1,color,"A");break;
 case 14:u->text(u->ctx,-2,-1,2,color,"AB");break;
 case 15:u->text(u->ctx,10,10,1,color,"\xe9\x9f\xb3");break; /* UTF-8 U+97F3 */
 case 16:u->text(u->ctx,10,10,1,color,"???");break;
 case 17:u->text(u->ctx,10,10,1,color,"?");break;
 }
 uint32_t hash=2166136261U,count=0,foreign=0;
 for(unsigned y=0;y<172;y++)for(unsigned x=0;x<320;x++){
  uint16_t value=pixels[y*stride+x];if(value==color)count++;else if(value)foreign++;
  hash=(hash^(value&255))*16777619U;hash=(hash^(value>>8))*16777619U;
 }
 hashes[index]=hash;int ok=!foreign&&(expected<0||count==(unsigned)expected);bad+=!ok;
 P->format(line,sizeof line,"R4UI RESULT case=%u count=%u fnv=%u foreign=%u rc=%d",index,count,hash,foreign,ok?0:-2);P->log(line);
 P->screen_present(0,0,320,172);
 if(++index==18){P->format(line,sizeof line,"R4UI SUMMARY cases=18 bad=%u utf8_matches_3q=%u utf8_matches_1q=%u rc=%d",bad,hashes[15]==hashes[16],hashes[15]==hashes[17],bad?-2:0);P->log(line);active=0;}
 return true;
}
}
