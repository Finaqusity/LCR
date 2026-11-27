#include "lcr_journal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[2][2048], baseline[2][2048];
static int operations, fail_at, read_error;
static int read_io(void *ctx,unsigned bank,size_t offset,uint8_t *out,size_t size) {
    (void)ctx; if(read_error)return 0;
    assert(bank<2u && offset+size<=2048u); memcpy(out,flash[bank]+offset,size);return 1;
}
static int erase_io(void *ctx,unsigned bank) {
    (void)ctx; if(operations++==fail_at)return 0; memset(flash[bank],0xff,2048u);return 1;
}
static int word_io(void *ctx,unsigned bank,size_t offset,uint32_t value) {
    (void)ctx; if(operations++==fail_at)return 0;
    for(unsigned i=0u;i<4u;++i) {
        const uint8_t byte=(uint8_t)(value>>(i*8u));
        assert((flash[bank][offset+i]&byte)==byte);flash[bank][offset+i]=byte;
    } return 1;
}
int main(void) {
    lcr_cal_profile p,loaded;
    const lcr_journal_io io={0,read_io,erase_io,word_io};
    unsigned bank=0u;
    memset(flash,0xff,sizeof(flash)); fail_at=-1;
    lcr_cal_profile_nominal(&p,123u,456u,8000000.0);
    p.slots[0][0].revision=1u;
    assert(lcr_journal_save(io,&p));
    assert(p.sequence==1u && lcr_journal_load(io,123u,456u,&loaded,&bank)==1);
    assert(bank==0u); memcpy(baseline,flash,sizeof(flash));
    const int steps=(int)(LCR_CAL_BLOB_SIZE/4u)+2;
    for(int cut=0;cut<steps;++cut) {
        memcpy(flash,baseline,sizeof(flash));operations=0;fail_at=cut;
        p.slots[0][0].revision=2u;
        assert(!lcr_journal_save(io,&p));
        fail_at=-1;
        assert(lcr_journal_load(io,123u,456u,&loaded,&bank)==1);
        assert(loaded.slots[0][0].revision==1u && bank==0u);
    }
    memcpy(flash,baseline,sizeof(flash));operations=0;fail_at=-1;
    assert(lcr_journal_save(io,&p));
    assert(lcr_journal_load(io,123u,456u,&loaded,&bank)==1 && bank==1u && loaded.sequence==2u);
    flash[1][100]^=1u;
    assert(lcr_journal_load(io,123u,456u,&loaded,&bank)==1 && bank==0u);
    memset(flash,0xff,sizeof(flash));p.sequence=UINT32_MAX;
    assert(lcr_cal_encode(&p,flash[0],sizeof(flash[0]))==LCR_STORE_OK);
    assert(word_io(0,0u,LCR_CAL_BLOB_SIZE,LCR_JOURNAL_COMMIT));
    assert(lcr_journal_save(io,&p) && p.sequence==0u);
    assert(lcr_journal_load(io,123u,456u,&loaded,&bank)==1 && bank==1u && loaded.sequence==0u);
    read_error=1;operations=0;
    assert(!lcr_journal_save(io,&p) && operations==0); /* Read failures must not trigger erase. */
    puts("lcr_journal: 445 interrupted writes, corruption fallback, sequence wrap and read-error guards passed");
    return 0;
}
