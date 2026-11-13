/* MIT. Public API probes: owned event payloads, queue pressure, timers and heap. */
#ifndef RESONA_EVENT_CONTRACT_H
#define RESONA_EVENT_CONTRACT_H
#include <resona/platform.h>
#include <resona/libc.h>
static const resona_platform *event_contract_p;
static uint32_t event_contract_started, event_contract_received, event_contract_bad;
static uint32_t event_contract_counts[8], event_contract_active;
static const uint32_t event_contract_ids[8] = {0,1,2,3,4,5,0x7fffffffU,0xffffffffU};
static void event_contract_begin(const resona_platform *p) {
    event_contract_p=p;event_contract_started=p->millis();event_contract_received=event_contract_bad=0;
    memset(event_contract_counts,0,sizeof event_contract_counts);event_contract_active=1;
    /* Caller may already use allocation records. Count only successfully obtained slots. */
    void *blocks[65]={0};unsigned count=0;
    for(;count<65;count++){
        blocks[count]=p->alloc(32,16);if(!blocks[count])break;
        for(unsigned j=0;j<32;j++)if(((uint8_t*)blocks[count])[j])event_contract_bad++;
        memset(blocks[count],0xa5,32);
    }
    if(!count||count>64)event_contract_bad++;
    for(unsigned i=0;i<count;i++)p->free(blocks[i]);
    for(unsigned i=0;i<8;i++){
        if(p->timer_start(event_contract_ids[i],20+i,1))event_contract_bad++;
    }
    int full=p->timer_start(1234,20,1);
    int invalid=p->timer_start(event_contract_ids[0],0xffffffffU,1);
    if(full!=RESONA_ERR_NO_MEMORY||invalid!=RESONA_ERR_ARG)event_contract_bad++;
    if(p->timer_start(event_contract_ids[1],30,3))event_contract_bad++;
    p->timer_stop(event_contract_ids[2]);
    if(p->timer_start(event_contract_ids[2],RESONA_APP_TIMER_MAX_PERIOD_MS,1))event_contract_bad++;
    if(p->timer_start(event_contract_ids[3],0x80000000U,1)!=RESONA_ERR_ARG)event_contract_bad++;
    if(p->timer_start(event_contract_ids[4],0,1)!=RESONA_ERR_ARG)event_contract_bad++;
    /* Fill the queue with alternating MIDI/CTRL, then immediately reuse the stack. */
    for(unsigned i=0;i<64;i++){
        uint8_t data[256];for(unsigned j=0;j<256;j++)data[j]=(uint8_t)(i+j);
        data[0]=0x51;data[1]=0x46;data[2]=(uint8_t)i;
        resona_app_event e={0};e.kind=i&1?RESONA_APP_EV_MIDI:RESONA_APP_EV_CTRL;
        if(i&1){e.u.midi.port=RESONA_MIDI_USB;e.u.midi.data=data;e.u.midi.len=sizeof data;}
        else{e.u.ctrl.data=data;e.u.ctrl.len=sizeof data;}
        p->post(&e);memset(data,0xff,sizeof data);
    }
    char line[128];p->format(line,sizeof line,"EVENTS START alloc_slots=%u ninth_rc=%d invalid_replace_rc=%d",count,full,invalid);p->log(line);
}
static int event_contract_event(const resona_app_event *e) {
    if(!event_contract_active)return 0;
    if(e->kind==RESONA_APP_EV_TIMER){
        for(unsigned i=0;i<8;i++)if(e->u.timer.id==event_contract_ids[i]){event_contract_counts[i]++;return 1;}
    }
    if(e->kind==RESONA_APP_EV_MIDI||e->kind==RESONA_APP_EV_CTRL){
        const uint8_t *b=e->kind==RESONA_APP_EV_MIDI?e->u.midi.data:e->u.ctrl.data;
        unsigned n=e->kind==RESONA_APP_EV_MIDI?e->u.midi.len:e->u.ctrl.len;
        if(n!=256||b[0]!=0x51||b[1]!=0x46){event_contract_bad++;return 1;}
        unsigned index=b[2];
        if(index!=event_contract_received||e->kind!=(index&1?RESONA_APP_EV_MIDI:RESONA_APP_EV_CTRL))event_contract_bad++;
        for(unsigned j=3;j<n;j++)if(b[j]!=(uint8_t)(index+j)){event_contract_bad++;break;}
        event_contract_received++;return 1;
    }
    return 0;
}
static void event_contract_frame(void) {
    const resona_platform *p=event_contract_p;
    if(!event_contract_active||p->millis()-event_contract_started<800)return;
    const unsigned expected[8]={1,3,0,1,1,1,1,1};
    for(unsigned i=0;i<8;i++){p->timer_stop(event_contract_ids[i]);if(event_contract_counts[i]!=expected[i])event_contract_bad++;}
    if(event_contract_received<31||event_contract_received>32)event_contract_bad++;
    char line[128];p->format(line,sizeof line,"EVENTS SUMMARY rc=%d bad=%u delivered=%u timers=%u/%u/%u",event_contract_bad?-2:0,event_contract_bad,event_contract_received,event_contract_counts[0],event_contract_counts[1],event_contract_counts[2]);p->log(line);
    event_contract_active=0;
}
#endif
