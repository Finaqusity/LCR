#include "lcr_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void preview(const char *name, const uint8_t *pixels) {
    const char *directory = getenv("LCR_PREVIEW_DIR");
    if (!directory) return;
    char path[1024];
    (void)snprintf(path,sizeof(path),"%s/%s.pbm",directory,name);
    FILE *file = 0;
#ifdef _MSC_VER
    assert(fopen_s(&file,path,"wb")==0);
#else
    file=fopen(path,"wb");
#endif
    assert(file);
    (void)fprintf(file,"P4\n128 64\n");
    for (unsigned y=0u;y<64u;++y) for(unsigned x=0u;x<128u;x+=8u) {
        unsigned byte=0u;
        for(unsigned b=0u;b<8u;++b)
            if(pixels[(y/8u)*128u+x+b]&(1u<<(y%8u))) byte|=1u<<(7u-b);
        assert(fputc((int)byte,file)!=EOF);
    }
    assert(fclose(file)==0);
}
int main(void) {
    lcr_keys keys;
    /* Held at boot: the key is blocked, releasing it must not start anything. */
    lcr_keys_init(&keys,1u,0u);
    assert(lcr_keys_poll(&keys,1u,1000u)==0u);
    assert(lcr_keys_poll(&keys,0u,1001u)==0u);
    assert(lcr_keys_poll(&keys,0u,1030u)==0u);
    /* Short press: reported only after the double-click window expires. */
    lcr_keys_init(&keys,0u,0u);
    assert(lcr_keys_poll(&keys,1u,100u)==0u);
    assert(lcr_keys_poll(&keys,1u,125u)==0u); /* stable: pressed at 125 */
    assert(lcr_keys_poll(&keys,0u,200u)==0u);
    assert(lcr_keys_poll(&keys,0u,225u)==0u); /* release at 225, window open */
    assert(lcr_keys_poll(&keys,0u,524u)==0u);
    assert(lcr_keys_poll(&keys,0u,525u)==LCR_KEY_SHORT);
    /* Long press: one LCR_KEY_LONG, no short event on release. */
    lcr_keys_init(&keys,0u,0u);
    assert(lcr_keys_poll(&keys,1u,100u)==0u);
    assert(lcr_keys_poll(&keys,1u,125u)==0u);
    assert(lcr_keys_poll(&keys,1u,1324u)==0u);
    assert(lcr_keys_poll(&keys,1u,1325u)==LCR_KEY_LONG);
    assert(lcr_keys_poll(&keys,1u,1000u)==0u);
    assert(lcr_keys_poll(&keys,0u,1025u)==0u);
    assert(lcr_keys_poll(&keys,0u,1100u)==0u);
    /* Double click: two presses inside the window report DOUBLE only. */
    lcr_keys_init(&keys,0u,0u);
    assert(lcr_keys_poll(&keys,1u,100u)==0u);
    assert(lcr_keys_poll(&keys,1u,125u)==0u);
    assert(lcr_keys_poll(&keys,0u,150u)==0u);
    assert(lcr_keys_poll(&keys,0u,175u)==0u);
    assert(lcr_keys_poll(&keys,1u,300u)==0u);
    assert(lcr_keys_poll(&keys,1u,325u)==0u);
    assert(lcr_keys_poll(&keys,0u,400u)==0u);
    assert(lcr_keys_poll(&keys,0u,425u)==LCR_KEY_DOUBLE);
    assert(lcr_keys_poll(&keys,0u,800u)==0u);
    /* Tick wrap: the long-press timer must not be confused by 32-bit wrap. */
    lcr_keys_init(&keys,0u,UINT32_MAX-100u);
    (void)lcr_keys_poll(&keys,1u,UINT32_MAX-90u);
    (void)lcr_keys_poll(&keys,1u,UINT32_MAX-60u);
    assert(lcr_keys_poll(&keys,1u,1145u)==LCR_KEY_LONG);
    assert(lcr_keys_poll(&keys,1u,1150u)==0u);

    lcr_canvas canvas, restored;
    uint8_t wire[1024];
    for(unsigned i=0u;i<1024u;++i) canvas.pixels[i]=(uint8_t)(i*17u+i/128u);
    lcr_canvas_wire(&canvas,wire,0); /* Forward mounting: identity mapping. */
    assert(memcmp(canvas.pixels,wire,1024u)==0);
    lcr_canvas_wire(&canvas,wire,1); /* Rotated mounting: 180-degree mapping. */
    for(unsigned y=0u;y<64u;++y) for(unsigned x=0u;x<128u;++x) {
        const unsigned a=(canvas.pixels[(y/8u)*128u+x]>>(y%8u))&1u;
        const unsigned b=(wire[((63u-y)/8u)*128u+127u-x]>>((63u-y)%8u))&1u;
        assert(a==b);
    }
    memcpy(restored.pixels,wire,1024u); lcr_canvas_wire(&restored,wire,1);
    assert(memcmp(canvas.pixels,wire,1024u)==0);
    lcr_measurement m;
    memset(&m,0,sizeof(m)); m.valid=1u; m.actual_hz=1000.0; m.range_index=2u;
    const lcr_complex impedance={100.0,-100.0};
    assert(lcr_parameters_from_z(impedance,1000.0,&m.parameters)==LCR_OK);
    double series,parallel; const char *unit;
    assert(lcr_ui_value(&m,LCR_MODEL_SERIES,&series,&unit));
    assert(lcr_ui_value(&m,LCR_MODEL_PARALLEL,&parallel,&unit));
    assert(fabs(series/parallel-2.0)<1e-12 && strcmp(unit,"F")==0);
    lcr_ui_measure(&canvas,&m,LCR_MODEL_SERIES,"RUN",0); preview("oled_series",canvas.pixels);
    lcr_ui_measure(&canvas,&m,LCR_MODEL_PARALLEL,"HOLD",0); preview("oled_parallel",canvas.pixels);
    lcr_canvas_wire(&canvas,wire,0); preview("oled_wire_forward",wire);
    lcr_canvas_wire(&canvas,wire,1); preview("oled_wire_180",wire);
    lcr_ui_message(&canvas,"CONFIRM CAL","DISCONNECT DUT","KEY:CANCEL HOLD:GO"); preview("oled_calibration",canvas.pixels);
    lcr_ui_message(&canvas,"LCR STATUS","ADC / DMA ERROR","NO RESULT ACCEPTED"); preview("oled_error",canvas.pixels);
    char number[32];
    lcr_ui_number(number,sizeof(number),999.96,"OHM"); assert(strcmp(number,"1.000 kOHM")==0);
    lcr_ui_number(number,sizeof(number),10e-12,"F"); assert(strstr(number,"pF"));
    m.valid=0u; assert(!lcr_ui_value(&m,LCR_MODEL_SERIES,&series,&unit));
    puts("lcr_ui: single-key debounce/long/double, wrap, forward and 180-degree pixels passed");
    return 0;
}
