/* MIT. Public SDK storage contract, safe to run on an App's existing store. */
#ifndef RESONA_STORAGE_CONTRACT_H
#define RESONA_STORAGE_CONTRACT_H
#include <resona/platform.h>
#include <resona/libc.h>
static int resona_storage_contract(const resona_platform *p) {
    uint32_t n=p->app_store_size();
    unsigned failed=0,checks=0;
    uint8_t *before=(uint8_t*)p->alloc(n,16), *after=(uint8_t*)p->alloc(n,16);
    char line[120];
#define CHECK(label,condition) do { int ok=(condition); checks++; failed+=!ok; p->format(line,sizeof line,"STORAGE %s %s",label,ok?"PASS":"FAIL");p->log(line); } while(0)
    if(!before || !after || n<4) {if(before)p->free(before);if(after)p->free(after);return RESONA_ERR_NO_MEMORY;}
    int snapshot=p->app_store_read(0,before,n);
    CHECK("snapshot",snapshot==RESONA_OK);
    if(snapshot)goto done;
    {
        const uint32_t spans[][2]={{n,1},{n-1,2},{n+1,0},{n+1,1},
            {UINT32_MAX,1},{UINT32_MAX,2},{1,UINT32_MAX},{n,UINT32_MAX},{0,UINT32_MAX}};
        for(unsigned i=0;i<sizeof spans/sizeof spans[0];i++) {
            uint8_t buf[8];memset(buf,0xA5,sizeof buf);
            int rc=p->app_store_read(spans[i][0],buf+2,spans[i][1]);
            int intact=1;for(unsigned j=0;j<sizeof buf;j++)if(buf[j]!=0xA5)intact=0;
            p->format(line,sizeof line,"STORAGE read[%u] rc=%d unchanged=%d",i,rc,intact);p->log(line);
            CHECK("invalid-read",rc==RESONA_ERR_ARG && intact);
            rc=p->app_store_write(spans[i][0],buf+2,spans[i][1]);
            CHECK("invalid-write",rc==RESONA_ERR_ARG);
        }
        CHECK("failed-writes-preserve-store",p->app_store_read(0,after,n)==0 && !memcmp(before,after,n));
        CHECK("empty-read-start",p->app_store_read(0,after,0)==0);
        CHECK("empty-read-end",p->app_store_read(n,after,0)==0);
        CHECK("empty-write-start",p->app_store_write(0,after,0)==0);
        CHECK("empty-write-end",p->app_store_write(n,after,0)==0);
        CHECK("empty-preserves-store",p->app_store_read(0,after,n)==0 && !memcmp(before,after,n));
        uint8_t last=(uint8_t)(before[n-1]^0x5A), readback=0;
        CHECK("valid-write-last",p->app_store_write(n-1,&last,1)==0);
        CHECK("valid-read-last",p->app_store_read(n-1,&readback,1)==0 && readback==last);
        /* Always restore the byte, even when a preceding assertion failed. */
        CHECK("restore-last",p->app_store_write(n-1,before+n-1,1)==0);
        CHECK("all-data-restored",p->app_store_read(0,after,n)==0 && !memcmp(before,after,n));
    }
done:
    p->free(before);p->free(after);
    p->format(line,sizeof line,"STORAGE SUMMARY checks=%u failed=%u",checks,failed);p->log(line);
#undef CHECK
    return failed ? RESONA_ERR_STATE : RESONA_OK;
}
#endif
