// MIT. SDK-only diagnostics; no firmware source, debug UART or persistent setters.
#pragma once
#include <resona/platform.h>
#include <resona/libc.h>
namespace round3 {
static const resona_platform *P;
static unsigned fmtBad,fmtCases;
static struct {unsigned char before[8];char out[192];unsigned char after[8];} G;
static void prepare(){memset(&G,0xa5,sizeof G);}
static void check(unsigned impl,unsigned id,unsigned cap,const char *expected,int result){
 unsigned length=(unsigned)strlen(expected),written=cap?(length<cap-1?length:cap-1):0;
 int contents=!memcmp(G.out,expected,written)&&(!cap||G.out[written]==0),guard=1;
 for(unsigned i=0;i<8;i++)if(G.before[i]!=0xa5||G.after[i]!=0xa5)guard=0;
 for(unsigned i=cap;i<sizeof G.out;i++)if((unsigned char)G.out[i]!=0xa5)guard=0;
 int ok=contents&&guard&&result==(int)length;fmtBad+=!ok;fmtCases++;
 char line[128];P->format(line,sizeof line,"R3FMT impl=%u case=%u n=%d expected=%u content=%d guard=%d rc=%d",impl,id,result,length,contents,guard,ok?0:-2);P->log(line);
 if(!ok&&cap&&result>=0&&(unsigned)result<cap&&G.out[result]==0){P->log("R3FMT actual:");P->log(G.out);P->log("R3FMT expected:");P->log(expected);}
}
static int formatting(const resona_platform *p,unsigned extended){
 P=p;fmtBad=fmtCases=0;
 for(unsigned local=0;local<2;local++){
  unsigned cap=sizeof G.out;int n;
  #define FMT(...) (local?snprintf(G.out,cap,__VA_ARGS__):P->format(G.out,cap,__VA_ARGS__))
  if(!extended){
   prepare();n=FMT("%d/%i/%u/%x/%X",(-2147483647-1),-7,0xffffffffU,0xabcdefU,0xabcdefU);check(local,0,cap,"-2147483648/-7/4294967295/abcdef/ABCDEF",n);
   prepare();n=FMT("%+06d|%06d|%-6d",42,-42,42);check(local,1,cap,"+00042|-00042|42    ",n);
   prepare();n=FMT("[%6s][%-6s]","ab","ab");check(local,2,cap,"[    ab][ab    ]",n);
   prepare();n=FMT("100%%:%c",'Q');check(local,3,cap,"100%:Q",n);
   cap=5;prepare();n=FMT("abcdef");check(local,4,cap,"abcdef",n);
   cap=1;prepare();n=FMT("abcdef");check(local,5,cap,"abcdef",n);
   cap=sizeof G.out;prepare();n=FMT("[%6c][%-6c]",'Q','Q');check(local,6,cap,"[     Q][Q     ]",n);
  }else{
   cap=0;prepare();n=FMT("abcdef");check(local,7,cap,"abcdef",n);
   cap=sizeof G.out;prepare();n=FMT("%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u",0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,10U,11U,12U,13U,14U,15U);check(local,8,cap,"0/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15",n);
   char text[161];memset(text,'K',160);text[160]=0;cap=96;prepare();n=FMT("%s",text);check(local,9,cap,text,n);
   cap=12;prepare();n=FMT("%64u",42U);check(local,10,cap,"                                                              42",n);
  }
  #undef FMT
 }
 char line[100];P->format(line,sizeof line,"R3FMT SUMMARY extended=%u cases=%u bad=%u rc=%d",extended,fmtCases,fmtBad,fmtBad?-2:0);P->log(line);
 return fmtBad?RESONA_ERR_STATE:0;
}
static unsigned metadataActive,metadataIndex,metadataEntries,metadataPresets,metadataBad,kinds[5];
static resona_storage_info storage;
static int metadata(const resona_platform *p){
 P=p;metadataBad=metadataIndex=0;memset(kinds,0,sizeof kinds);
 resona_device_info d{};P->device_info(&d);resona_program_resources c{};P->program_resources(&c);
 if(d.sample_rate!=48000||d.block_frames!=64||d.screen_width!=320||d.screen_height!=172||d.encoders!=2||c.block_frames!=d.block_frames)metadataBad++;
 metadataEntries=P->entry_count();metadataPresets=RESONA_PROGRAM_PRESETS;P->storage_info(&storage);
 if(metadataEntries>256||metadataPresets!=50||storage.pool_free>storage.pool_bytes)return RESONA_ERR_STATE;
 for(unsigned id=1;id<=9;id++){int32_t value=0;int rc=P->setting_get(id,&value);char line[96];P->format(line,sizeof line,"R3META setting=%u rc=%d value=%d",id,rc,value);P->log(line);if(rc)metadataBad++;}
 resona_startup_info startup{};int rc=P->startup_info(&startup);if(rc)metadataBad++;
 resona_bluetooth_info bt{};int brc=P->bluetooth_names_get(&bt);
 char line[128];P->format(line,sizeof line,"R3META START entries=%u presets=%u pool=%u free=%u startup_rc=%d bt_rc=%d",metadataEntries,metadataPresets,storage.pool_bytes,storage.pool_free,rc,brc);P->log(line);
 metadataActive=1;return 0;
}
static resona_rgb_state saved;
static resona_rgb_config tested;
static unsigned rgbActive,rgbMode,rgbSamples,rgbBad;
static uint32_t rgbStart,rgbLast;
static uint8_t *largeInput,*largeOutput;
static unsigned largeActive,largeIndex,largeBad;
static int large_begin(const resona_platform *p){
 P=p;largeInput=(uint8_t*)P->alloc(8192,16);largeOutput=(uint8_t*)P->alloc(8193,16);
 if(!largeInput||!largeOutput){if(largeInput)P->free(largeInput);if(largeOutput)P->free(largeOutput);largeInput=largeOutput=nullptr;return RESONA_ERR_NO_MEMORY;}
 largeIndex=largeBad=0;largeActive=1;return 0;
}
static int rgb_invalid(const resona_platform *p){
 P=p;resona_rgb_state before{},after{};int rc=P->rgb_get(&before);if(rc)return rc;
 unsigned bad=0;
 for(unsigned i=0;i<8;i++){
  resona_rgb_config c=resona_rgb_solid(1,2,3,7);
  switch(i){case 0:c.version++;break;case 1:c.size--;break;case 2:c.reserved=1;break;case 3:c.animation=5;break;case 4:c.animation=1;break;case 5:c.animation=1;c.period_ms=19;break;case 6:c.animation=1;c.period_ms=60001;break;case 7:c.period_ms=1;break;}
  int result=P->rgb_set(&c);int read=P->rgb_get(&after);int kept=!read&&!memcmp(&after.config,&before.config,sizeof c)&&after.owner==before.owner;
  if(result!=RESONA_ERR_ARG||!kept)bad++;
  char line[96];P->format(line,sizeof line,"R3RGB REJECT case=%u rc=%d state_kept=%d",i,result,kept);P->log(line);
 }
 rc=P->rgb_set(before.owner==RESONA_RGB_OWNER_APP?&before.config:nullptr);if(rc)bad++;
 char line[96];P->format(line,sizeof line,"R3RGB REJECT-SUMMARY cases=8 bad=%u rc=%d",bad,bad?-2:0);P->log(line);return bad?RESONA_ERR_STATE:0;
}
static int eq_roundtrip(const resona_platform *p){
 P=p;uint32_t oldOn=0,on=0;int8_t oldBands[5]={},oldLevel=0,bands[5]={},level=0;
 int rc=P->global_eq_get(&oldOn,oldBands,&oldLevel);if(rc)return rc;unsigned bad=0;
 const int8_t test[5]={-30,-15,0,15,30};
 rc=P->global_eq_set(1,test,-24,0);int read=P->global_eq_get(&on,bands,&level);
 if(rc||read||on!=1||level!=-24||memcmp(bands,test,5))bad++;
 char line[128];P->format(line,sizeof line,"R3EQ valid_rc=%d read_rc=%d enabled=%u level=%d bad=%u",rc,read,on,level,bad);P->log(line);
 for(unsigned i=0;i<6;i++){
  int8_t b[5];memcpy(b,test,5);uint32_t enabled=1;int8_t l=-24;
  switch(i){case 0:enabled=2;break;case 1:b[0]=-31;break;case 2:b[4]=31;break;case 3:l=-25;break;case 4:l=25;break;case 5:enabled=0;b[2]=127;break;}
  rc=P->global_eq_set(enabled,b,l,0);read=P->global_eq_get(&on,bands,&level);
  int kept=!read&&on==1&&level==-24&&!memcmp(bands,test,5);if(rc!=RESONA_ERR_ARG||!kept)bad++;
  P->format(line,sizeof line,"R3EQ REJECT case=%u rc=%d state_kept=%d",i,rc,kept);P->log(line);
 }
 int restore=P->global_eq_set(oldOn,oldBands,oldLevel,0);read=P->global_eq_get(&on,bands,&level);
 int kept=!restore&&!read&&on==oldOn&&level==oldLevel&&!memcmp(bands,oldBands,5);if(!kept)bad++;
 P->format(line,sizeof line,"R3EQ SUMMARY cases=8 restored=%d bad=%u rc=%d",kept,bad,bad?-2:0);P->log(line);return bad?RESONA_ERR_STATE:0;
}
static int rgb_begin(const resona_platform *p){
 P=p;int rc=P->rgb_get(&saved);if(rc)return rc;
 rgbMode=rgbSamples=rgbBad=0;rgbActive=1;rgbStart=rgbLast=P->millis();
 tested=resona_rgb_solid(157,63,211,101);rc=P->rgb_set(&tested);
 if(rc)rgbActive=0;return rc;
}
static void frame(const resona_platform *p){
 P=p;
 if(largeActive){
  const unsigned lengths[]={255,256,511,512,1023,1024,2047,2048,4095,4096,8191};unsigned n=lengths[largeIndex];
  memset(largeInput,'K',n);largeInput[n]=0;memset(largeOutput,0xa5,8193);
  char line[128];P->format(line,sizeof line,"R3BIG BEGIN index=%u input_len=%u cap=%u",largeIndex,n,n+1);P->log(line);
  int result=P->format((char*)largeOutput,n+1,"%s",(char*)largeInput);
  int data=result==(int)n&&largeOutput[n]==0&&largeOutput[n+1]==0xa5;
  for(unsigned i=0;i<n&&data;i++)if(largeOutput[i]!='K')data=0;
  P->format(line,sizeof line,"R3BIG RESULT input_len=%u cap=%u return=%d data_ok=%d",n,n+1,result,data);P->log(line);
  if(!data)largeBad++;
  if(largeBad||++largeIndex==sizeof lengths/sizeof lengths[0]){
   P->free(largeInput);P->free(largeOutput);largeInput=largeOutput=nullptr;largeActive=0;
   P->format(line,sizeof line,"R3BIG SUMMARY tested=%u bad=%u rc=%d",largeIndex+(largeBad?1:0),largeBad,largeBad?-2:0);P->log(line);
  }
 }
 if(metadataActive){
  if(metadataIndex<metadataEntries){resona_entry_info e{};int rc=P->entry_info(metadataIndex,&e);if(rc||e.kind<1||e.kind>4||RESONA_ENTRY_KIND(e.id)!=e.kind)metadataBad++;else kinds[e.kind]++;}
  else if(metadataIndex<metadataEntries+metadataPresets){resona_preset_info preset{};if(P->preset_info(metadataIndex-metadataEntries,&preset))metadataBad++;}
  else{
   resona_entry_info e{};resona_preset_info preset{};
   if(P->entry_info(metadataEntries,&e)!=RESONA_ERR_ARG||P->preset_info(metadataPresets,&preset)!=RESONA_ERR_ARG)metadataBad++;
   for(unsigned i=1;i<5;i++)if(kinds[i]!=storage.used[i])metadataBad++;
   char line[128];P->format(line,sizeof line,"R3META SUMMARY ead=%u nam=%u ir=%u packs=%u bad=%u rc=%d",kinds[1],kinds[2],kinds[3],kinds[4],metadataBad,metadataBad?-2:0);P->log(line);metadataActive=0;
  }metadataIndex++;
 }
 if(rgbActive&&P->millis()-rgbLast>=40){
  rgbLast=P->millis();resona_rgb_state got{};int rc=P->rgb_get(&got);uint8_t expected[3];resona_rgb_evaluate(&tested,got.phase_ms,expected);
  int valid=!rc&&got.version==RESONA_RGB_VERSION&&got.size==sizeof got&&!memcmp(&tested,&got.config,sizeof tested)&&got.owner==RESONA_RGB_OWNER_APP&&got.red==expected[0]&&got.green==expected[1]&&got.blue==expected[2];
  if(!valid){rgbBad++;char line[128];P->format(line,sizeof line,"R3RGB MISMATCH mode=%u phase=%u rgb=%u/%u/%u expected=%u/%u/%u rc=%d",rgbMode,got.phase_ms,got.red,got.green,got.blue,expected[0],expected[1],expected[2],rc);P->log(line);}
  rgbSamples++;
  if(P->millis()-rgbStart>=500){
   if(rgbMode==RESONA_RGB_FADE&&(!(got.flags&RESONA_RGB_FINISHED)||got.phase_ms!=tested.period_ms))rgbBad++;
   char line[128];P->format(line,sizeof line,"R3RGB MODE mode=%u samples=%u bad=%u phase=%u flags=%u",rgbMode,rgbSamples,rgbBad,got.phase_ms,got.flags);P->log(line);
   if(++rgbMode>RESONA_RGB_FADE){
    int restored=P->rgb_set(saved.owner==RESONA_RGB_OWNER_APP?&saved.config:nullptr);if(restored)rgbBad++;
    P->format(line,sizeof line,"R3RGB SUMMARY samples=%u bad=%u restore_rc=%d rc=%d",rgbSamples,rgbBad,restored,rgbBad?-2:0);P->log(line);rgbActive=0;
   }else{
    tested.animation=rgbMode;tested.period_ms=200;tested.red2=13;tested.green2=201;tested.blue2=44;
    if(P->rgb_set(&tested))rgbBad++;rgbStart=P->millis();
   }
  }
 }
}
}
