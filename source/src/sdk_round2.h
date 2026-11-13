// MIT. SDK-only event, timer and numerical-routing tests; no firmware access.
#pragma once
#include <resona/platform.h>
#include <resona/libc.h>
namespace round2 {
static const resona_platform *P;
static uint8_t packets[64][8],large[256];
static uint32_t postStarted,postMode,postCount,postReceived,postBad,postOrder,postActive;
static uint32_t timerStarted,timerActive,timerMode,timerCounts[9],timerBad;
static const uint32_t ids[9]={0,1,2,3,4,5,0x7fffffffU,0xffffffffU,7};
static void log(const char*label,int rc,uint32_t extra=0){char s[128];P->format(s,sizeof s,"ROUND2 %s rc=%d extra=%u",label,rc,extra);P->log(s);}
static int post(const resona_platform*p,unsigned mode){
 P=p;postMode=mode;postCount=mode==2?64:1;postReceived=postBad=postOrder=0;postStarted=P->millis();postActive=1;
 for(unsigned i=0;i<postCount;i++){
  uint8_t *b=mode==1?large:packets[i];unsigned n=mode==1?256:8;
  for(unsigned j=0;j<n;j++)b[j]=(uint8_t)((i+j)&127);
  b[0]=0x51;b[1]=0x45;b[2]=(uint8_t)mode;b[3]=(uint8_t)i;
  resona_app_event e{};e.kind=RESONA_APP_EV_CTRL;e.u.ctrl.data=b;e.u.ctrl.len=n;P->post(&e);
  b[4]^=255; // valid stable memory, deliberately changed after post returns
 }
 log("POST-START",0,postCount);return 0;
}
static int timers(const resona_platform*p,unsigned mode){
 P=p;for(unsigned i=0;i<9;i++){P->timer_stop(ids[i]);timerCounts[i]=0;}
 P->timer_stop(0x12340001);P->timer_stop(0x12340002);timerBad=0;timerMode=mode;timerStarted=P->millis();timerActive=1;
 if(mode==1){
  int a=P->timer_start(0x12340001,0x80000000U,1),b=P->timer_start(0x12340002,0xffffffffU,1);
  char s[128];P->format(s,sizeof s,"ROUND2 LONG-TIMER-START half_rc=%d max_rc=%d",a,b);P->log(s);
  if(a==0)timerBad|=0x100;if(b==0)timerBad|=0x200;return 0;
 }
 for(unsigned i=0;i<8;i++)if(P->timer_start(ids[i],20+i,1))timerBad++;
 int replacement=P->timer_start(ids[0],30,2),overflow=P->timer_start(ids[8],40,1);
 if(replacement||overflow>=0)timerBad++;
 P->timer_stop(ids[1]);if(P->timer_start(ids[8],40,1))timerBad++;
 P->timer_stop(ids[2]);if(P->timer_start(ids[2],20,3))timerBad++;
 int zero=P->timer_start(123456,0,1);if(zero!=RESONA_ERR_ARG)timerBad++;
 char s[128];P->format(s,sizeof s,"ROUND2 TIMER-CAPACITY replace_rc=%d ninth_rc=%d zero_period_rc=%d",replacement,overflow,zero);P->log(s);return 0;
}
static bool event(const resona_app_event*e){
 if(postActive&&e->kind==RESONA_APP_EV_CTRL&&e->u.ctrl.len>=8){
  const uint8_t*b=e->u.ctrl.data;unsigned n=e->u.ctrl.len;
  if(b[0]!=0x51||b[1]!=0x45)return false;
  unsigned index=b[3];if(index!=postReceived)postOrder++;
  if(n!=(postMode==1?256U:8U)||b[2]!=postMode)postBad++;
  for(unsigned j=4;j<n;j++)if(b[j]!=(uint8_t)((index+j)&127)){postBad++;break;}
  postReceived++;return true;
 }
 if(timerActive&&e->kind==RESONA_APP_EV_TIMER){
  uint32_t id=e->u.timer.id;
  if(timerMode==1&&(id==0x12340001||id==0x12340002)){
   unsigned slot=id==0x12340001?0:1;timerCounts[slot]++;
   char s[128];P->format(s,sizeof s,"ROUND2 LONG-TIMER-FIRED id=%u elapsed_ms=%u",id,P->millis()-timerStarted);P->log(s);return true;
  }
  for(unsigned i=0;i<9;i++)if(id==ids[i]){timerCounts[i]++;return true;}
 }
 return false;
}
static void frame(const resona_platform*p){
 P=p;char s[128];
 if(postActive&&P->millis()-postStarted>=600){
  postActive=0;int ok=postReceived>0&&postReceived<=32&&!postBad&&!postOrder&&(postCount>32||postReceived==postCount);
  P->format(s,sizeof s,"ROUND2 POST-SUMMARY mode=%u sent=%u received=%u bad=%u order=%u rc=%d",postMode,postCount,postReceived,postBad,postOrder,ok?0:-2);P->log(s);
 }
 if(timerActive&&P->millis()-timerStarted>=800){
  timerActive=0;
  if(timerMode==1){
   P->timer_stop(0x12340001);P->timer_stop(0x12340002);
   P->format(s,sizeof s,"ROUND2 LONG-TIMER-SUMMARY half_fired=%u max_fired=%u accepted_mask=%u",timerCounts[0],timerCounts[1],timerBad);P->log(s);
  }else{
   const unsigned expected[9]={2,0,3,1,1,1,1,1,1};
   for(unsigned i=0;i<9;i++){P->timer_stop(ids[i]);if(timerCounts[i]!=expected[i])timerBad++;}
   P->format(s,sizeof s,"ROUND2 TIMER-SUMMARY rc=%d bad=%u zero_id=%u max_id=%u",timerBad?-2:0,timerBad,timerCounts[0],timerCounts[7]);P->log(s);
  }
 }
}
static int routing(const resona_platform*p,unsigned mode,uint32_t trim){
 P=p;resona_effect_route r[6];unsigned n=0;int param=0;
 auto edge=[&](uint32_t src,uint32_t dst,float gain,unsigned sc,unsigned dc){r[n++]={src,dst,gain,(uint8_t)sc,(uint8_t)dc,0};};
 if(mode==9){
  edge(RESONA_EFFECT_USB,RESONA_EFFECT_CAPTURE,1,0,0);edge(RESONA_EFFECT_USB,RESONA_EFFECT_CAPTURE,-1,0,1);
 }else if(mode==0||mode==1||mode==3||mode==4){
  float g=mode==3?-1.f:mode==4?0.f:1.f;
  edge(RESONA_EFFECT_USB,RESONA_EFFECT_CAPTURE,g,0,mode==1?1:0);edge(RESONA_EFFECT_USB,RESONA_EFFECT_CAPTURE,g,1,mode==1?0:1);
 }else if(mode==2){
  for(unsigned dc=0;dc<2;dc++){edge(RESONA_EFFECT_USB,RESONA_EFFECT_CAPTURE,.5f,0,dc);edge(RESONA_EFFECT_USB,RESONA_EFFECT_CAPTURE,.5f,1,dc);}
 }else{
  float value=mode==5?.8f:mode==6?-10.f:10.f;
  if(mode==8){uint32_t bits=0x7fc00000;memcpy(&value,&bits,4);}
  param=P->effect_param_set(trim,0,value);
  if(mode==8){if(param!=RESONA_ERR_ARG)return -2;}else if(param)return param;
  if(mode==5){edge(RESONA_EFFECT_USB,trim,1,0,0);edge(RESONA_EFFECT_USB,trim,1,1,0);}else edge(RESONA_EFFECT_USB,trim,1,0,0);
  edge(trim,RESONA_EFFECT_CAPTURE,mode==5?.5f:1.f,0,0);edge(trim,RESONA_EFFECT_CAPTURE,mode==5?.5f:1.f,0,1);
 }
 resona_effect_graph graph={RESONA_EFFECTS_VERSION,sizeof graph,n,r};int rc=P->effects_graph_set(&graph);
 char s[128];P->format(s,sizeof s,"ROUND2 ROUTING mode=%u graph_rc=%d param_rc=%d",mode,rc,param);P->log(s);return rc;
}
}
