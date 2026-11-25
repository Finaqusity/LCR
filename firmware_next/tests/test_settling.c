#include "lcr_session.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned muted,relays,frequencies;uint32_t token; } mock;
static uint16_t pairs[2400];
static int mute_io(void*ctx,int muted) { ((mock*)ctx)->muted=(unsigned)muted;return 1; }
static void relay_io(void*ctx,uint8_t mask) { mock*m=ctx;assert(m->muted);++m->relays;(void)mask; }
static int freq_io(void*ctx,uint32_t hz) { mock*m=ctx;assert(m->muted);++m->frequencies;(void)hz;return 1; }
static int capture_io(void*ctx,unsigned f,unsigned r,size_t count,uint32_t token) {
    ((mock*)ctx)->token=token;(void)f;(void)r;(void)count;return 1;
}
static void cancel_io(void*ctx) { (void)ctx; }
/* v1.dc is the controlled variable: the settling gate compares it against
   LCR_STABILITY_DC_CODES and the source amplitude against the relative gate. */
static void waveform(unsigned dc) {
    const double omega=lcr_sample_omega(&lcr_frequencies[3]);
    for(unsigned n=0u;n<1200u;++n) {
        pairs[2u*n]=(uint16_t)(dc+600.0*cos(n*omega)+0.5);
        pairs[2u*n+1u]=(uint16_t)(1900.0-600.0*cos(n*omega)+0.5);
    }
}
static void init(mock*m,lcr_session*s) {
    memset(m,0,sizeof(*m));
    const lcr_frontend_io fe={m,mute_io,relay_io};
    const lcr_session_io io={fe,m,freq_io,capture_io,cancel_io,0};
    assert(lcr_session_init(s,io));s->adaptive_settling=1u;s->keep_excitation=1u;
}
int main(void) {
    mock m;lcr_session s;init(&m,&s);
    uint32_t now=UINT32_MAX-100u;
    assert(lcr_session_start(&s,3u,2u,0,8000000.0,now));
    unsigned frames=0u;
    while(s.state!=LCR_SESSION_DONE && s.state!=LCR_SESSION_FAILED) {
        ++now;lcr_session_poll(&s,now);
        if(s.state==LCR_SESSION_CAPTURING) {
            /* Two frames at 2200, then a step to 2100: the step resets the
               stable counter and only two identical comparisons finish. */
            waveform(frames<2u?2200u:2100u);
            ++frames;assert(lcr_session_submit(&s,m.token,pairs,1200u,now));
            if(frames<5u)assert(!s.result.valid && s.state==LCR_SESSION_SWITCHING);
        }
    }
    assert(s.state==LCR_SESSION_DONE && frames==5u && s.result.valid && !m.muted);
    const unsigned relays=m.relays,freqs=m.frequencies;
    assert(lcr_session_start(&s,3u,2u,0,8000000.0,now));
    unsigned reused=0u;
    while(s.state!=LCR_SESSION_DONE && s.state!=LCR_SESSION_FAILED && reused<4u) {
        lcr_session_poll(&s,now);
        if(s.state==LCR_SESSION_CAPTURING) {
            waveform(2100u);
            assert(lcr_session_submit(&s,m.token,pairs,1200u,now));
            ++reused;
        }
        ++now;
    }
    assert(s.state==LCR_SESSION_DONE && reused<4u && m.relays==relays && m.frequencies==freqs);
    lcr_session_stop(&s);assert(m.muted);

    init(&m,&s);now=0u;frames=0u;
    assert(lcr_session_start(&s,3u,2u,0,8000000.0,now));
    while(s.state!=LCR_SESSION_FAILED && now<1400u) {
        ++now;lcr_session_poll(&s,now);
        if(s.state==LCR_SESSION_CAPTURING) {
            waveform(2100u+100u*(frames%5u)); ++frames;
            (void)lcr_session_submit(&s,m.token,pairs,1200u,now);
            assert(!s.result.valid);
        }
    }
    assert(s.state==LCR_SESSION_FAILED && s.error==LCR_SESSION_UNSTABLE && m.muted);
    puts("lcr_settling: DC transients rejected, two stable comparisons required, deadline/wrap and continuous reuse passed");
    return 0;
}
