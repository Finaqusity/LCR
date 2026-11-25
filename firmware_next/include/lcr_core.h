#ifndef LCR_CORE_H
#define LCR_CORE_H

#include <stddef.h>
#include <stdint.h>

/* CPU-only maths. No peripheral access, allocation, or legacy application code. */
typedef struct { double re, im; } lcr_complex;
typedef enum {
    LCR_OK = 0,
    LCR_BAD_INPUT,
    LCR_SINGULAR,
    LCR_CLIPPED,
    LCR_WEAK_SIGNAL,
    LCR_NO_EXCITATION
} lcr_status;

typedef struct {
    lcr_complex phasor; /* x[n] = dc + Re(phasor * exp(j*omega*n)) */
    double dc;
    double residual_rms;
    uint16_t minimum, maximum;
    uint8_t clipped;
} lcr_fit;

typedef struct {
    uint32_t nominal_hz;
    uint32_t timer_divisor;
    uint16_t sample_pairs;
} lcr_frequency;

extern const lcr_frequency lcr_frequencies[4];
extern const uint8_t lcr_range_masks[6];

uint32_t lcr_dds_word(uint32_t hz);
double lcr_sample_omega(const lcr_frequency *frequency);
lcr_status lcr_fit_adc(const uint16_t *samples, size_t count, size_t stride,
                       double omega, lcr_fit *out);
lcr_status lcr_feedback(double rf, double cf, double hz, lcr_complex *out);
lcr_status lcr_impedance(const lcr_fit *v1, const lcr_fit *v2,
                         lcr_complex zf, lcr_complex alpha, lcr_complex *out);
/* Like lcr_impedance, but allow_weak keeps frames whose SNR is low: the
   returned status is still LCR_WEAK_SIGNAL so callers can mark confidence. */
lcr_status lcr_impedance_ex(const lcr_fit *v1, const lcr_fit *v2,
                            lcr_complex zf, lcr_complex alpha, int allow_weak,
                            lcr_complex *out);
lcr_status lcr_open_admittance(const lcr_fit *v1, const lcr_fit *v2,
                               lcr_complex zf, lcr_complex alpha, lcr_complex *out);
lcr_status lcr_compensate(lcr_complex zm, lcr_complex zs,
                          lcr_complex yo, lcr_complex *out);

#endif
