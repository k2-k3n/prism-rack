// MIT. Public effect identity/constructor and parameter-metadata probes.
#pragma once
#include <resona/platform.h>
#include <resona/factory_params.h>
#include <resona/libc.h>
#include "round5_crc.h"
namespace round5 {
static const resona_platform *P;
static unsigned active,index,bad;
static uint32_t bits(float value){uint32_t result;memcpy(&result,&value,4);return result;}
static int metadata(const resona_platform*p,uint32_t h,unsigned tag,unsigned variant=0){
 P=p;resona_effect_info info{};int rc=P->effect_info(h,&info);if(rc)return rc;
 char line[128];P->format(line,sizeof line,"R5META tag=%u kind=%u engine=%u params=%u rc=%d",tag,info.kind,info.engine,info.param_count,rc);P->log(line);
 if(info.param_count>12)return -2;
 unsigned errors=0;
 for(unsigned i=0;i<info.param_count;i++){
  resona_effect_param v{};rc=P->effect_param_info(h,i,&v);if(rc){errors++;continue;}
  char name[17],unit[9];memcpy(name,v.name,16);name[16]=0;memcpy(unit,v.unit,8);unit[8]=0;
  P->format(line,sizeof line,"R5PARAM tag=%u index=%u name=%s unit=%s flags=%u",tag,i,name,unit,v.flags);P->log(line);
  P->format(line,sizeof line,"R5RANGE tag=%u index=%u min=%u max=%u def=%u step=%u",tag,i,bits(v.min),bits(v.max),bits(v.def),bits(v.step));P->log(line);
  if(!(v.min<=v.max&&v.def>=v.min&&v.def<=v.max&&v.step>=0))errors++;
  if(info.kind==RESONA_EFFECT_FACTORY){resona_effect_param want{};int match=resona_factory_param_info(info.engine,variant,i,&want)==0&&
   !strcmp(name,want.name)&&!strcmp(unit,want.unit)&&bits(v.min)==bits(want.min)&&bits(v.max)==bits(want.max)&&bits(v.def)==bits(want.def)&&bits(v.step)==bits(want.step)&&v.flags==want.flags;
   if(!match){errors++;P->format(line,sizeof line,"R5META CONTRACT MISMATCH tag=%u index=%u",tag,i);P->log(line);}}

 }
 P->format(line,sizeof line,"R5META SUMMARY tag=%u bad=%u rc=%d",tag,errors,errors?-2:0);P->log(line);return errors?-2:0;
}
static int begin(const resona_platform*p){P=p;active=1;index=bad=0;P->log("R5CREATE START cases=19");return 0;}
static int setters(const resona_platform*p){
 P=p;resona_effect_source s;resona_effect_source_init(&s,RESONA_EFFECT_FACTORY);s.engine=RESONA_ENGINE_GATE;
 resona_effects_status before{},after{};P->effects_status(&before);uint32_t h=0;int rc=P->effect_create(&s,&h);if(rc)return rc;
 metadata(P,h,20);char line[128];resona_effect_param v{};
 const uint32_t tests[]={0x3ec00000U,0xbe800000U,0x3fa00000U,0x7fc00000U,0x7f800000U};
 for(unsigned i=0;i<5;i++){
  float value;memcpy(&value,&tests[i],4);rc=P->effect_param_set(h,0,value);
  int read=P->effect_param_info(h,0,&v);
  P->format(line,sizeof line,"R5SET case=%u input_bits=%u set_rc=%d info_rc=%d def_bits=%u",i,tests[i],rc,read,bits(v.def));P->log(line);
 }
 int invalid=P->effect_param_set(h,0xffffffffU,.5f);int cleanup=P->effect_destroy(h);P->effects_status(&after);
 int kept=!cleanup&&before.instances==after.instances&&before.routes==after.routes&&after.active&&!after.faults;
 P->format(line,sizeof line,"R5SET SUMMARY invalid_index_rc=%d cleanup_rc=%d graph_kept=%d rc=%d",invalid,cleanup,kept,kept?0:-2);P->log(line);
 return kept?0:-2;
}
static void frame(const resona_platform*p){
 if(!active)return;P=p;
 resona_effect_source s;resona_effect_source_init(&s,RESONA_EFFECT_PACK);
 memcpy(s.id,"org.resona.demo.prism-trim",sizeof "org.resona.demo.prism-trim");
 memcpy(s.package_version,"0.1.0",sizeof "0.1.0");s.crc32=ROUND5_TRIM_CRC;
 bool wantCreate=index<3||index==15||index==18;
 switch(index){
 case 1:s.package_version[0]=0;break;
 case 2:s.crc32=0;break;
 case 3:s.crc32^=0x01010101U;break;
 case 4:memcpy(s.package_version,"999.0",6);break;
 case 5:memcpy(s.id,"org.resona.test.missing",sizeof "org.resona.test.missing");break;
 case 6:s.id[0]=0;break;
 case 7:s.version++;break;
 case 8:s.size--;break;
 case 9:s.kind=99;break;
 case 10:s.reserved[0]=1;break;
 case 11:memset(s.id,'A',sizeof s.id);break;
 case 12:memset(s.package_version,'A',sizeof s.package_version);break;
 case 16:memcpy(s.id,"org.resona.example.delay",sizeof "org.resona.example.delay");s.crc32=0;break;
 case 17:case 18:memcpy(s.id,"org.resona.example.gain",sizeof "org.resona.example.gain");s.crc32=index==18?ROUND5_GAIN_CRC:0;break;
 case 13:case 14:case 15:
  resona_effect_source_init(&s,RESONA_EFFECT_FACTORY);s.engine=index==14?0:RESONA_ENGINE_GATE;s.variant=index==13?2:index==15?1:0;break;
 }
 resona_effects_status before{},after{},clean{};P->effects_status(&before);uint32_t h=0;
 char line[128];P->format(line,sizeof line,"R5CREATE BEGIN case=%u want_create=%u",index,wantCreate?1:0);P->log(line);
 int rc=P->effect_create(&s,&h);P->effects_status(&after);
 int ok=wantCreate?(rc==0&&h&&after.instances==before.instances+1):(rc<0&&!h&&after.instances==before.instances);
 if(before.routes!=after.routes||!after.active||after.faults)ok=0;
 int destroy=0;
 if(h){resona_effect_info info{};if(P->effect_info(h,&info)==0){
   if(index==0||index==15)metadata(P,h,index==0?10:11,index==15?1:0);
   destroy=P->effect_destroy(h);
  }}
 P->effects_status(&clean);
 if(destroy||clean.instances!=before.instances||clean.routes!=before.routes||!clean.active||clean.faults)ok=0;
 P->format(line,sizeof line,"R5CODE case=%u name=%s",index,resona_error_string(rc));P->log(line);
 P->format(line,sizeof line,"R5CREATE RESULT case=%u raw_rc=%d handle=%u added=%u destroy_rc=%d kept=%u rc=%d",index,rc,h,after.instances-before.instances,destroy,clean.instances==before.instances&&clean.routes==before.routes,ok?0:-2);P->log(line);
 int wanted=index==16?RESONA_ERR_PACK_TARGET:index==17?RESONA_ERR_PACK_AMBIGUOUS:index==3?RESONA_ERR_PACK_CRC:index==4?RESONA_ERR_PACK_VERSION:index==5?RESONA_ERR_PACK_ID:wantCreate?0:RESONA_ERR_ARG;
 if(rc!=wanted){ok=0;P->format(line,sizeof line,"R5CODE WRONG case=%u got=%d expected=%d",index,rc,wanted);P->log(line);}
 bad+=!ok;
 if(!ok||++index==19){P->format(line,sizeof line,"R5CREATE SUMMARY completed=%u bad=%u rc=%d",index+(!ok?1:0),bad,bad?-2:0);P->log(line);active=0;}
}
}
