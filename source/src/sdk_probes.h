// MIT. Bounded public SDK probes. No firmware debug APIs or private transport.
#pragma once
#include <resona/effects.hpp>
#include <resona/libc.h>
namespace sdk_probe {
static const resona_platform *P;
static unsigned counts[4], timerActive, timerErrors;
static uint32_t timerStart;
static void message(const char *name,int rc,int extra=0) {
 char s[128];P->format(s,sizeof s,"SDKTEST %s rc=%d extra=%d",name,rc,extra);P->log(s);
}
static unsigned heap_slots() {
 void *ptrs[80]={};unsigned n=0;
 for(;n<80;n++){ptrs[n]=P->alloc(32,16);if(!ptrs[n])break;}
 for(unsigned i=0;i<n;i++)P->free(ptrs[i]);
 return n;
}
static int allocation(const resona_platform *p) {
 P=p;unsigned before=heap_slots(),fail=0;
 for(unsigned align=1;align<=4096;align*=2){
  uint8_t *b=(uint8_t*)P->alloc(257,align);
  int ok=b && !((uintptr_t)b&(align-1));
  if(b){memset(b,0xA7,257);for(unsigned i=0;i<257;i++)if(b[i]!=0xA7)ok=0;P->free(b);}
  char row[128];P->format(row,sizeof row,"SDKTEST ALIGN align=%u allocated=%d aligned=%d",align,b!=nullptr,ok);P->log(row);if((align<=128&&!ok)||(b&&!ok))fail++;
 }
 for(unsigned i=0;i<64;i++){
  void *a=P->alloc(8192,16),*b=P->alloc(1024,32);if(!a||!b)fail++;
  if(a)P->free(a);if(b)P->free(b);
 }
 const unsigned sizes[]={16384,32768,49152,65536,131072};
 for(unsigned n:sizes){void *b=P->alloc(n,16);message("ALLOC-SIZE",b?0:RESONA_ERR_NO_MEMORY,n);if(b)P->free(b);}
 unsigned after=heap_slots();message("HEAP-SLOTS-BEFORE",0,before);message("HEAP-SLOTS-AFTER",0,after);
 if(after!=before)fail++;
 message("ALLOC-SUMMARY",fail?RESONA_ERR_STATE:0,fail);return fail?RESONA_ERR_STATE:0;
}
static int timer_start(const resona_platform *p) {
 P=p;memset(counts,0,sizeof counts);timerErrors=0;
 for(unsigned i=0;i<4;i++)P->timer_stop(100+i);
 timerErrors+=P->timer_start(100,20,3)!=0;
 timerErrors+=P->timer_start(101,15,0)!=0;
 timerErrors+=P->timer_start(102,40,1)!=0;P->timer_stop(102);
 timerErrors+=P->timer_start(103,500,1)!=0;
 timerErrors+=P->timer_start(103,60,2)!=0;
 timerStart=P->millis();timerActive=1;message("TIMERS-START",timerErrors? -2:0);return timerErrors?-2:0;
}
static void event(const resona_app_event *e) {
 if(timerActive&&e->kind==RESONA_APP_EV_TIMER&&e->u.timer.id>=100&&e->u.timer.id<104){
  unsigned i=e->u.timer.id-100;counts[i]++;if(i==1&&counts[i]==5)P->timer_stop(101);
 }
}
static void frame(const resona_platform *p) {
 P=p;if(!timerActive||P->millis()-timerStart<700)return;
 timerActive=0;for(unsigned i=0;i<4;i++)P->timer_stop(100+i);
 char s[128];int ok=!timerErrors&&counts[0]==3&&counts[1]==5&&counts[2]==0&&counts[3]==2;
 P->format(s,sizeof s,"SDKTEST TIMER-SUMMARY rc=%d finite=%u forever-stopped=%u cancelled=%u replaced=%u",ok?0:-2,counts[0],counts[1],counts[2],counts[3]);P->log(s);
}
static int lifecycle(const resona_platform *p) {
 P=p;unsigned fail=0;resona_effects_status before{},after{};P->effects_status(&before);
 resona_effect_source src;resona_effect_source_init(&src,RESONA_EFFECT_PACK);
 memcpy(src.id,"org.resona.demo.prism-trim",sizeof "org.resona.demo.prism-trim");
 memcpy(src.package_version,"0.1.0",sizeof "0.1.0");
 uint32_t h=0;int rc=P->effect_create(&src,&h);message("CREATE-DISCONNECTED",rc,h);if(rc||!h)return rc?rc:-2;
 resona_effect_info info{};rc=P->effect_info(h,&info);if(rc||info.param_count!=1)fail++;
 resona_effect_param param{};rc=P->effect_param_info(h,0,&param);message("PARAM-METADATA",rc,info.param_count);if(rc)fail++;
 rc=P->effect_param_set(h,0,.8f);if(rc)fail++;
 rc=P->effect_enable(h,0);if(rc)fail++;rc=P->effect_enable(h,1);if(rc)fail++;
 rc=P->effect_destroy(h);message("DESTROY-DISCONNECTED",rc);if(rc)fail++;
 rc=P->effect_info(h,&info);message("STALE-INFO",rc);if(rc>=0)fail++;
 rc=P->effect_param_set(h,0,.5f);message("STALE-PARAM",rc);if(rc>=0)fail++;
 rc=P->effect_enable(h,1);message("STALE-ENABLE",rc);if(rc>=0)fail++;
 rc=P->effect_destroy(h);message("STALE-DESTROY",rc);if(rc>=0)fail++;
 P->effects_status(&after);if(before.instances!=after.instances||before.routes!=after.routes||!after.active||after.faults)fail++;
 message("LIFECYCLE-SUMMARY",fail?-2:0,fail);return fail?-2:0;
}
static int rejection(const resona_platform *p,unsigned test,uint32_t gate,uint32_t trim) {
 P=p;resona_effects_status before{},after{};P->effects_status(&before);
 resona_effect_route routes[2]={{RESONA_EFFECT_GUITAR,trim,1,0,0,0},{trim,RESONA_EFFECT_OUTPUT,1,0,0,0}};
 resona_effect_graph graph={RESONA_EFFECTS_VERSION,sizeof(graph),2,routes};
 float nan,inf;uint32_t bits=0x7fc00000;memcpy(&nan,&bits,4);bits=0x7f800000;memcpy(&inf,&bits,4);
 resona_effect_route *oversized=nullptr;
 int rc=0;
 message("BEGIN-REJECTION",0,test);
 switch(test){
 case 20:routes[1]=routes[0];break;
 case 21:routes[0]={trim,trim,1,0,0,0};break;
 case 22:routes[0].source=0x123456;break;
 case 23:routes[0].destination=RESONA_EFFECT_GUITAR;break;
 case 24:routes[0].source_channel=1;break;
 case 25:routes[0].destination_channel=1;break;
 case 26:routes[0].gain=nan;break;
 case 27:routes[0].gain=inf;break;
 case 28:routes[0].gain=32768;break;
 case 29:routes[0]={RESONA_EFFECT_MAIN,RESONA_EFFECT_OUTPUT,1,0,0,0};break;
 case 30:graph.version++;break;
 case 31:graph.size--;break;
 case 32:graph.route_count=RESONA_EFFECT_MAX_ROUTES+1;
  oversized=(resona_effect_route*)P->alloc(graph.route_count*sizeof(*oversized),16);
  if(!oversized)return RESONA_ERR_NO_MEMORY;
  memset(oversized,0,graph.route_count*sizeof(*oversized));graph.routes=oversized;break;
 case 35:rc=P->effect_param_set(trim,0,nan);goto checked;
 case 36:rc=P->effect_param_set(trim,0xffffffffu,1);goto checked;
 case 37:rc=P->effect_enable(0x123456,1);goto checked;
 case 38:rc=P->effect_destroy(gate);goto checked;
 case 39:{resona_effect_info i{};rc=P->effect_info(0,&i);goto checked;}
 default:return -1;
 }
 rc=P->effects_graph_set(&graph);
 if(oversized)P->free(oversized);
checked:
 P->effects_status(&after);
 int kept=before.generation==after.generation&&before.instances==after.instances&&before.routes==after.routes&&after.active&&!after.faults;
 char s[128];P->format(s,sizeof s,"SDKTEST REJECTION case=%u raw_rc=%d graph_kept=%d",test,rc,kept);P->log(s);
 return rc<0&&kept?0:-2;
}
static bool midi_event(const resona_platform *p,const resona_app_event *e) {
 if(e->kind!=RESONA_APP_EV_MIDI||e->u.midi.len<7)return false;
 const uint8_t *m=e->u.midi.data;
 if(m[0]!=0xf0||m[1]!=0x7d||m[2]!=0x53||m[3]!=0x44)return false;
 P=p;uint32_t crc=2166136261u;for(unsigned i=0;i<e->u.midi.len;i++)crc=(crc^m[i])*16777619u;
 char line[128];P->format(line,sizeof line,"SDKTEST MIDI-RX token=%u len=%u last=%u fnv=%u",m[4],e->u.midi.len,m[e->u.midi.len-1],crc);P->log(line);
 uint8_t reply[]={0xf0,0x7d,0x53,0x45,m[4],(uint8_t)(e->u.midi.len&127),(uint8_t)(e->u.midi.len>>7),(uint8_t)(m[e->u.midi.len-1]&127),0xf7};
 int rc=P->midi_send(RESONA_MIDI_USB,reply,sizeof reply);if(rc)message("MIDI-RX-ACK",rc);return true;
}
static int midi_send(const resona_platform *p,unsigned len) {
 P=p;uint8_t *bytes=(uint8_t*)P->alloc(len,16);if(!bytes)return RESONA_ERR_NO_MEMORY;
 for(unsigned i=0;i<len;i++)bytes[i]=(uint8_t)(i%127);
 bytes[0]=0xf0;bytes[1]=0x7d;bytes[2]=0x53;bytes[3]=0x46;bytes[4]=(uint8_t)(len&127);bytes[5]=(uint8_t)(len>>7);bytes[len-1]=0xf7;
 message("MIDI-TX-BEGIN",0,len);int rc=P->midi_send(RESONA_MIDI_USB,bytes,len);
 message("MIDI-TX-RETURN",rc,len);P->free(bytes);return rc;
}
static int builder(const resona_platform *p) {
 P=p;unsigned before=heap_slots();resona_effects_status a{},b{};P->effects_status(&a);
 {resona::Effects temporary;int rc=temporary.begin(P);if(rc){message("SECOND-CONTROLLER-REFUSED",rc);return rc;}
  rc=temporary.connect(RESONA_EFFECT_GUITAR,RESONA_EFFECT_OUTPUT);if(rc)return rc;}
 unsigned after=heap_slots();P->effects_status(&b);
 char s[128];P->format(s,sizeof s,"SDKTEST BUILDER-SCOPE slots_before=%u slots_after=%u gen_before=%u gen_after=%u instances=%u active=%u",before,after,a.generation,b.generation,b.instances,b.active);P->log(s);
 return before==after&&a.generation==b.generation&&a.instances==b.instances&&b.active?0:-2;
}
}
