/* MIT. Portable character-format and non-persistent EQ regression probes. */
#ifndef RESONA_FORMAT_EQ_CONTRACT_H
#define RESONA_FORMAT_EQ_CONTRACT_H
#include <resona/platform.h>
#include <resona/libc.h>
static int resona_format_eq_contract(const resona_platform *p) {
    unsigned format_checks=0,format_bad=0,eq_checks=0,eq_bad=0;
    const unsigned characters[3]={0,'Q',255};
    for(unsigned local=0;local<2;local++)for(unsigned left=0;left<2;left++)
    for(unsigned c=0;c<3;c++)for(unsigned cap=0;cap<=20;cap++){
        struct {uint8_t before[8];char text[24];uint8_t after[8];} g;
        memset(&g,0xa5,sizeof g);char expected[6];memset(expected,' ',6);
        expected[left?0:5]=(char)characters[c];
        const char *fmt=left?"%-6c":"%6c";
        int n=local?snprintf(g.text,cap,fmt,(int)characters[c]):p->format(g.text,cap,fmt,(int)characters[c]);
        unsigned wrote=cap?(cap-1<6?cap-1:6):0;
        int ok=n==6&&!memcmp(g.text,expected,wrote)&&(!cap||!g.text[wrote]);
        for(unsigned i=0;i<8;i++)if(g.before[i]!=0xa5||g.after[i]!=0xa5)ok=0;
        for(unsigned i=cap;i<sizeof g.text;i++)if((uint8_t)g.text[i]!=0xa5)ok=0;
        format_checks++;if(!ok)format_bad++;
    }
    char line[128];p->format(line,sizeof line,"FIXTEST FORMAT checks=%u bad=%u rc=%d",format_checks,format_bad,format_bad?-2:0);p->log(line);
    uint32_t saved_on=0,on=0;int8_t saved[5]={0},saved_level=0,read[5]={0},read_level=0;
    int rc=p->global_eq_get(&saved_on,saved,&saved_level);
    if(rc)return rc;
    const int8_t bands[5]={-30,-15,0,15,30};
    for(unsigned enabled=0;enabled<=1;enabled++){
        int8_t level=enabled?24:-24;
        rc=p->global_eq_set(enabled,bands,level,0);
        int rr=p->global_eq_get(&on,read,&read_level);
        eq_checks++;if(rc||rr||on!=enabled||read_level!=level||memcmp(read,bands,5))eq_bad++;
        for(unsigned test=0;test<8;test++){
            int8_t request[5];memcpy(request,bands,5);
            request[0]=-29;request[1]=-14; /* valid changes preceding the invalid field */
            uint32_t flag=enabled,persist=0;int8_t lev=level;
            switch(test){
            case 0:flag=2;break;
            case 1:flag=0xffffffffU;break;
            case 2:persist=2;break;
            case 3:persist=0xffffffffU;break;
            case 4:request[4]=31;break;
            case 5:request[4]=-31;break;
            case 6:lev=25;break;
            case 7:lev=-25;break;
            }
            rc=p->global_eq_set(flag,request,lev,persist);
            rr=p->global_eq_get(&on,read,&read_level);eq_checks++;
            if(rc!=RESONA_ERR_ARG||rr||on!=enabled||read_level!=level||memcmp(read,bands,5))eq_bad++;
        }
    }
    rc=p->global_eq_set(saved_on,saved,saved_level,0);
    int rr=p->global_eq_get(&on,read,&read_level);
    int restored=!rc&&!rr&&on==saved_on&&read_level==saved_level&&!memcmp(saved,read,5);
    eq_checks++;if(!restored)eq_bad++;
    p->format(line,sizeof line,"FIXTEST EQ checks=%u bad=%u restored=%d rc=%d",eq_checks,eq_bad,restored,eq_bad?-2:0);p->log(line);
    return format_bad||eq_bad?RESONA_ERR_STATE:RESONA_OK;
}
#endif
