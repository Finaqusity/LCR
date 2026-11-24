#include "lcr_app.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    lcr_app *app;
    unsigned muted,active,frequency,range,starts,saves;
    uint8_t mask;
    uint32_t now,due,token;
    double resistance,cf,gain,phase;
    unsigned component;
    double component_value;
} simulator;
static lcr_app app;
static lcr_cal_profile profile,baseline;
static uint16_t samples[2400];
static int mute_io(void *ctx,int muted) { ((simulator*)ctx)->muted=(unsigned)muted;return 1; }
static void relays_io(void *ctx,uint8_t mask) { simulator*s=ctx;assert(s->muted && !s->active);s->mask=mask; }
static int frequency_io(void *ctx,uint32_t hz) { simulator*s=ctx;assert(s->muted && !s->active);(void)hz;return 1; }
static int capture_io(void *ctx,unsigned f,unsigned r,size_t count,uint32_t token) {
    simulator*s=ctx;assert(!s->active && !s->muted);
    s->active=1u;s->frequency=f;s->range=r;s->token=token;++s->starts;
    s->due=s->now+((uint32_t)count*lcr_frequencies[f].timer_divisor+83999u)/84000u;
    return 1;
}
static void cancel_io(void *ctx) { ((simulator*)ctx)->active=0u; }
static int save_io(void *ctx,lcr_cal_profile*p) { simulator*s=ctx;assert(s->muted && !s->active);++s->saves;++p->sequence;return 1; }
static uint16_t code(double value) { return value<0?0u:value>4095?4095u:(uint16_t)(value+0.5); }
static void frame(simulator*s) {
    const lcr_frequency*f=&lcr_frequencies[s->frequency];
    const double hz=8000000.0*lcr_dds_word(f->nominal_hz)/268435456.0;
    const double omega=6.2831853071795864769*hz*f->timer_divisor/84000000.0;
    double re2,im2;
    if(s->mask&0x40u) { re2=600.0;im2=0.0; }
    else {
        const double r=lcr_nominal_rf[s->range]*1.001;
        const double wrc=6.2831853071795864769*hz*r*s->cf;
        const double re=r/(1.0+wrc*wrc),im=-r*wrc/(1.0+wrc*wrc);
        if(s->resistance>1e15) {
            const double yr=1e-10,yi=6.2831853071795864769*hz*1e-12;
            re2=-600.0*(re*yr-im*yi);im2=-600.0*(re*yi+im*yr);
        } else {
            const double x=s->component==1u?-1.0/(6.2831853071795864769*hz*s->component_value):
                           s->component==2u?6.2831853071795864769*hz*s->component_value:0.0;
            const double norm=s->resistance*s->resistance+x*x;
            re2=-600.0*(re*s->resistance+im*x)/norm;
            im2=-600.0*(im*s->resistance-re*x)/norm;
        }
    }
    const double re1=600.0*s->gain*cos(s->phase),im1=600.0*s->gain*sin(s->phase);
    for(unsigned n=0u;n<f->sample_pairs;++n) {
        samples[2u*n]=code(2100.0+re1*cos(n*omega)-im1*sin(n*omega));
        samples[2u*n+1u]=code(1900.0+re2*cos(n*omega)-im2*sin(n*omega));
    }
}
static void step(simulator*s) {
    ++s->now;lcr_app_poll(s->app,0u,s->now);
    if(s->active && (int32_t)(s->now-s->due)>=0) {
        frame(s);lcr_app_submit(s->app,s->token,samples,lcr_frequencies[s->frequency].sample_pairs,s->now);
    }
    s->app->dirty=0u;
}
static void cal_start(simulator*s,unsigned menu) {
    lcr_app_event(s->app,LCR_KEY_LONG,s->now);   /* Stop and open the menu. */
    s->app->page=LCR_PAGE_MENU;s->app->menu=menu;
    lcr_app_event(s->app,LCR_KEY_LONG,s->now);   /* Activate the menu entry. */
    assert(s->app->page==LCR_PAGE_CONFIRM);
    lcr_app_event(s->app,LCR_KEY_LONG,s->now);   /* Execute after confirmation. */
    assert(s->app->page==LCR_PAGE_CAL);
}
static void cal_finish(simulator*s) {
    const uint32_t end=s->now+100000u;
    while(s->app->page==LCR_PAGE_CAL && s->now<end)step(s);
    if(strcmp(s->app->message,"CAL COMPLETE IN RAM")!=0)
        fprintf(stderr,"cal failure: %s / %s at f%u r%u\n",s->app->message,s->app->message2,s->app->cal_frequency,s->app->cal_range);
    assert(s->app->page==LCR_PAGE_MESSAGE && strcmp(s->app->message,"CAL COMPLETE IN RAM")==0);
    assert(s->muted && !s->active && s->app->unsaved);
}
int main(void) {
    simulator sim;memset(&sim,0,sizeof(sim));sim.app=&app;sim.resistance=1000.0;sim.cf=1e-12;sim.gain=1.0;
    const lcr_frontend_io frontend={&sim,mute_io,relays_io};
    const lcr_session_io acquisition={frontend,&sim,frequency_io,capture_io,cancel_io,0};
    const lcr_app_io io={acquisition,&sim,save_io};
    lcr_cal_profile_nominal(&profile,123u,456u,8000000.0);
    assert(lcr_app_init(&app,io,&profile,0u,0u));
    lcr_app_event(&app,LCR_KEY_SHORT,0u);
    while(!app.last.valid && sim.now<20000u)step(&sim);
    assert(app.last.valid && app.running && !app.scan && app.active_frequency==1u);
    assert(fabs(app.last.corrected_z.re-1000.0)<3.0);
    const unsigned starts=sim.starts;const lcr_measurement held=app.last;
    lcr_app_event(&app,LCR_KEY_DOUBLE,sim.now);
    assert(app.model==LCR_MODEL_PARALLEL && sim.starts==starts && memcmp(&held,&app.last,sizeof(held))==0);
    lcr_app_event(&app,LCR_KEY_SHORT,sim.now);
    assert(!app.running && sim.muted && !sim.active && app.last.valid);
    for(unsigned kind=1u;kind<=2u;++kind) {
        const double values[3]={kind==1u?10e-6:100e-3,kind==1u?1e-9:100e-6,kind==1u?10e-12:10e-6};
        for(unsigned n=0u;n<3u;++n) {
            sim.component=kind;sim.component_value=values[n];sim.resistance=0.3;
            app.last.valid=0u;lcr_app_event(&app,LCR_KEY_SHORT,sim.now);
            const uint32_t deadline=sim.now+30000u;
            while(!app.last.valid && sim.now<deadline)step(&sim);
            assert(app.last.valid && app.running && app.last.parameters.kind==(kind==1u?LCR_COMPONENT_C:LCR_COMPONENT_L));
            const double value=kind==1u?app.last.parameters.cs_f:app.last.parameters.ls_h;
            assert(fabs(value/values[n]-1.0)<0.005);
            lcr_app_event(&app,LCR_KEY_SHORT,sim.now);
            assert(!app.running && sim.muted);
        }
    }
    sim.component=0u;sim.resistance=1000.0;
    const unsigned stopped_starts=sim.starts;
    lcr_app_event(&app,LCR_KEY_LONG,sim.now);app.menu=5u;
    lcr_app_event(&app,LCR_KEY_LONG,sim.now);
    assert(app.page==LCR_PAGE_MESSAGE && sim.starts==stopped_starts);
    sim.gain=1.02;sim.phase=0.03;
    cal_start(&sim,2u);cal_finish(&sim);
    for(unsigned f=0u;f<4u;++f)for(unsigned r=0u;r<6u;++r) {
        assert(app.profile.slots[f][r].flags==LCR_CAL_CHANNEL);
        assert(fabs(app.profile.slots[f][r].alpha.re-sim.gain*cos(sim.phase))<0.002);
    }
    for(unsigned i=0u;i<5u;++i)step(&sim);
    assert(!app.last.valid); /* A calibration phasor must not become a DUT result. */
    baseline=app.profile;
    cal_start(&sim,2u);for(unsigned i=0u;i<900u;++i)step(&sim);
    lcr_app_event(&app,LCR_KEY_SHORT,sim.now);
    assert(memcmp(&baseline,&app.profile,sizeof(baseline))==0);
    sim.resistance=1e18;
    cal_start(&sim,3u);cal_finish(&sim);
    for(unsigned f=0u;f<4u;++f)for(unsigned r=0u;r<6u;++r)
        assert((app.profile.slots[f][r].flags&(LCR_CAL_CHANNEL|LCR_CAL_FIXTURE))==(LCR_CAL_CHANNEL|LCR_CAL_FIXTURE));
    sim.resistance=1000.0;sim.cf=1e-9;app.range_mode=3u;
    cal_start(&sim,4u);cal_finish(&sim);
    for(unsigned f=0u;f<4u;++f) {
        assert(app.profile.slots[f][2].flags==(LCR_CAL_CHANNEL|LCR_CAL_FEEDBACK));
        assert(fabs(app.profile.slots[f][2].rf_ohm-1001.0)<3.0);
    }
    app.page=LCR_PAGE_MENU;app.menu=6u;
    lcr_app_event(&app,LCR_KEY_LONG,sim.now);
    assert(sim.saves==1u && !app.unsaved && sim.muted);
    baseline=app.profile;
    cal_start(&sim,2u);
    while(app.cal_samples==0u && app.page==LCR_PAGE_CAL)step(&sim);
    sim.gain=1.10;
    const uint32_t deadline=sim.now+5000u;
    while(app.page==LCR_PAGE_CAL && sim.now<deadline)step(&sim);
    assert(app.page==LCR_PAGE_MESSAGE && strcmp(app.message,"CAL UNSTABLE")==0);
    assert(memcmp(&baseline,&app.profile,sizeof(baseline))==0 && sim.muted);
    puts("lcr_app: single key, model hold, frequency search, continuous measure, channel/open/standard wizards and save passed");
    return 0;
}
