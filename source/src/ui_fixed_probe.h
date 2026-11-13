// MIT. Public SDK regression and App-owned font showcase; no persistent writes.
#pragma once
#include <resona/font.h>
#include <resona/ui.h>
#include <resona/libc.h>
#include "../assets/fonts/demo_font.h"
#include "demo_capture.h"
namespace ui_fixed {
static unsigned active,index,bad;
static uint32_t hashes[17],holdUntil;
static int begin(const resona_platform*p){
 if(!resona_font_valid(&kDemoFont))return RESONA_ERR_ARG;
 active=1;index=bad=holdUntil=0;memset(hashes,0,sizeof hashes);p->log("UIFIX BEGIN app_owned_font=1");return 0;
}
static int show(const resona_platform*p){
 if(!resona_font_valid(&kDemoFont))return RESONA_ERR_ARG;
 active=2;index=17;holdUntil=0;p->log("UIFIX SHOWCASE LIVE (CC28=89 returns to normal)");return 0;
}
static void stop(){active=0;holdUntil=0;}
static void showcase(const resona_ui*u){
 u->clear(u->ctx,RESONA_RGB(12,18,24));
 u->text(u->ctx,16,12,2,RESONA_RGB(210,220,230),"APP-OWNED FONT");
 resona_ui_text_font(u,16,54,3,RESONA_RGB(60,230,200),"音量 50%",&kDemoFont);
 resona_ui_dots(u,264,76,24,24,0,360,8,8,3,RESONA_RGB(255,200,100));
 u->text(u->ctx,16,126,1,RESONA_RGB(150,160,170),"FONT IN APP / API 8 / NO FLASH WRITE");
}
static bool frame(const resona_platform*p){
 if(!active)return false;
 if(demo_capture::paused(p))return true;
 if(active==2){
  if(holdUntil&&p->millis()-holdUntil<100)return true;
  uint16_t*px=nullptr;uint32_t stride=0;resona_ui*u=p->screen_begin(&px,&stride);
  if(u&&px){showcase(u);p->screen_present(0,0,320,172);demo_capture::remember(px,stride);}
  holdUntil=p->millis();return true;
 }
 if(index>16){if((int32_t)(p->millis()-holdUntil)<0)return true;active=0;return false;}
 uint16_t*px=nullptr;uint32_t stride=0;resona_ui*u=p->screen_begin(&px,&stride);
 if(!u||!px||stride<320){active=0;p->log("UIFIX SCREEN FAILED rc=-2");return true;}
 u->clear(u->ctx,0);const uint16_t color=0xffe0;int expected=-1,rc=0;
 char text[70];int gx=10,gy=10;unsigned scale=1;
 switch(index){
 case 0:u->text(u->ctx,10,10,1,color,"音");expected=9;break;
 case 1:u->text(u->ctx,10,10,1,color,"?");expected=9;break;
 case 2:u->text(u->ctx,10,10,1,color,"???");expected=27;break;
 case 3:u->dots(u->ctx,40,40,15,15,0,360,8,7,1,color);expected=7;break;
 case 4:u->dots(u->ctx,40,40,15,15,0,360,8,8,1,color);expected=8;break;
 case 5:u->dots(u->ctx,40,40,15,15,0,360,2,2,1,color);expected=2;break;
 case 6:u->dots(u->ctx,40,40,15,15,0,180,2,2,1,color);expected=2;break;
 case 7:u->dots(u->ctx,40,40,15,15,0,-360,8,8,1,color);expected=8;break;
 case 8:rc=resona_ui_text_font(u,10,10,1,color,"音",&kDemoFont);if(rc!=14)bad++;break;
 case 9:rc=resona_ui_text_font(u,10,10,1,color,"音量 50%",&kDemoFont);if(rc!=52)bad++;break;
 case 10:gx=-3;gy=-2;rc=resona_ui_text_font(u,gx,gy,1,color,"音",&kDemoFont);if(rc!=14)bad++;break;
 case 11:gx=308;gy=160;scale=2;rc=resona_ui_text_font(u,gx,gy,scale,color,"音",&kDemoFont);if(rc!=28)bad++;break;
 case 12:u->text(u->ctx,10,10,1,color,"\xe2\x82" "A");break;
 case 13:u->text(u->ctx,10,10,1,color,"?A");break;
 case 14:memset(text,'A',62);memcpy(text+62,"音B",5);u->text(u->ctx,-360,10,1,color,text);break;
 case 15:memset(text,'A',62);text[62]=0;u->text(u->ctx,-360,10,1,color,text);break;
 case 16:showcase(u);break;
 }
 unsigned count=0,foreign=0,mismatch=0;uint32_t hash=2166136261U;
 const resona_font_glyph*g=&kDemoFontGlyphs[1];
 for(unsigned y=0;y<172;y++)for(unsigned x=0;x<320;x++){
  uint16_t value=px[y*stride+x];count+=value==color;foreign+=value&&value!=color;
  hash=(hash^(value&255))*16777619U;hash=(hash^(value>>8))*16777619U;
  if(index==8||index==10||index==11){
   int64_t xx=(int64_t)x-gx,yy=(int64_t)y-gy;int lit=0;
   if(xx>=0&&yy>=0&&xx<12*scale&&yy<12*scale){unsigned col=(unsigned)xx/scale,row=(unsigned)yy/scale;lit=(kDemoFontBitmap[g->offset+row*2+col/8]&(0x80u>>(col%8)))!=0;}
   mismatch+=value!=(lit?color:0);
  }
 }
 hashes[index]=hash;int ok=index==16||(!foreign&&!mismatch&&(expected<0||count==(unsigned)expected));bad+=!ok;
 char line[128];p->format(line,sizeof line,"UIFIX RESULT case=%u count=%u fnv=%u mismatch=%u rc=%d",index,count,hash,mismatch,ok?0:-2);p->log(line);
 p->screen_present(0,0,320,172);demo_capture::remember(px,stride);
 if(++index==17){
  if(hashes[0]!=hashes[1]||hashes[0]==hashes[2]||hashes[3]==hashes[4]||hashes[12]!=hashes[13]||hashes[14]!=hashes[15])bad++;
  p->format(line,sizeof line,"UIFIX SUMMARY cases=17 bad=%u font_valid=1 rc=%d",bad,bad?-2:0);p->log(line);holdUntil=p->millis()+15000;
 }
 return true;
}
}
