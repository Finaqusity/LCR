/* ADC1+ADC2 双重规则同步采集（§4.4、§5.1）。
 * TIM2_TRGO（EXTSEL=0b0110）同时触发两个 ADC，DMA2_Stream0 以 32 位读 ADC->CDR
 * 搬进 SRAM 的 .dma_buf（循环 + 半满/全满双缓冲）。ISR 只计数与查错，计算全在主循环。
 *
 * 勘误：架构文档 §4.4/§13.2 的 DMA DIR=01 是 memory-to-peripheral，
 * 本实现用 DIR=00（peripheral-to-memory），依据 stm32f4xx_hal_dma.h:227。 */

#include "adc.h"
#include "board.h"
#include "dds.h"

/* DMA 不能访问 CCM，必须放 SRAM；32 B 对齐、NOLOAD、KEEP（§8.1）。 */
static uint32_t adc_dma_buf[2U * ADC_N_MAX]
    __attribute__((section(".dma_buf"), aligned(32), used));

static volatile uint32_t adc_events = 0U;    /* 半满/全满事件总数 */
static volatile uint32_t adc_ovr = 0U;       /* 累计 OVR 次数 */
static volatile uint32_t adc_dmaerr = 0U;
static volatile uint32_t adc_awd = 0U;
static volatile int      adc_running = 0;
static volatile int      adc_need_reinit = 0;

static uint32_t adc_consumed = 0U;           /* 主循环已消费的事件数 */
static uint16_t adc_n = 0U;
static uint32_t adc_fs = 0U;
static uint32_t adc_arr = 0U;
static int      adc_awd_on = 0;
static uint32_t adc_ovr_streak = 0U;

static const cfg_band_t adc_bands[CFG_FREQ_NUM] = CFG_BANDS_INITIALIZER;

uint32_t adc_fs_from_arr(uint32_t arr)
{
    return CFG_TIMCLK1_HZ / (arr + 1U);
}

int adc_make_cfg(uint32_t band, uint16_t n, acq_cfg_t *cfg)
{
    if ((band >= CFG_FREQ_NUM) || (cfg == 0)) {
        return -1;
    }
    const cfg_band_t *b = &adc_bands[band];
    cfg->f0_hz = b->f_nom;
    cfg->s = b->s;
    cfg->n = (n == 0U) ? b->n_fine : n;
    cfg->fs_hz = adc_fs_from_arr(b->arr);
    return 0;
}

static int adc_cfg_valid(const acq_cfg_t *cfg)
{
    if ((cfg == 0) || (cfg->n == 0U) || (cfg->n > ADC_N_MAX) ||
        (cfg->s == 0U) || (cfg->s > ADC_S_MAX) ||
        ((cfg->n % cfg->s) != 0U) ||
        (cfg->fs_hz == 0U) || (cfg->fs_hz > CFG_ADC_FS_MAX)) {
        return 0;
    }
    return 1;
}

/* ---- 硬件配置 ------------------------------------------------------------ */
static void adc_gpio_init(void)
{
    GPIO_TypeDef *port = board_gpio(CFG_ADC_VIN_PORT);
    const uint32_t pins[2] = { CFG_ADC_VIN_PIN, CFG_ADC_VO_PIN };
    for (uint32_t i = 0U; i < 2U; ++i) {
        const uint32_t p = pins[i];
        port->MODER |= (3UL << (p * 2U));        /* 模拟态 */
        port->PUPDR &= ~(3UL << (p * 2U));       /* 无上下拉 */
    }
}

static void adc_core_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN;
    (void)RCC->APB2ENR;

    /* 公共：MULTI=0b00110 常规同步、ADCPRE=0b01(PCLK2/4=21 MHz)、
     * DMA=0b10(mode 2：CDR 双半字成对打包，一次 32 位请求) */
    ADC->CCR = (ADC->CCR & ~(ADC_CCR_MULTI | ADC_CCR_ADCPRE | ADC_CCR_DMA))
             | ADC_CCR_MULTI_1 | ADC_CCR_MULTI_2
             | ADC_CCR_ADCPRE_0
             | ADC_CCR_DMA_1;

    /* EXTSEL=0b0110 = TIM2_TRGO，EXTEN=0b01 上升沿；12bit、单通道、右对齐 */
    const uint32_t cr2 = ADC_CR2_ADON | ADC_CR2_EOCS
                       | ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2 | ADC_CR2_EXTEN_0;
    ADC1->CR1 = 0U;
    ADC2->CR1 = 0U;
    ADC1->CR2 = cr2 | ADC_CR2_DMA;      /* 只有主 ADC 发 DMA 请求 */
    ADC2->CR2 = cr2;                    /* 从 ADC 的 CR2.DMA 保持 0 */

    ADC1->SQR1 = 0U;                    /* L=0：1 次转换 */
    ADC2->SQR1 = 0U;
    ADC1->SQR3 = 0U;                    /* SQ1 = IN0 (PA0, Vin) */
    ADC2->SQR3 = 1U;                    /* SQ1 = IN1 (PA1, Vo) */
    ADC1->SMPR2 = 0U;                   /* SMP0 = 000 → 3 周期 */
    ADC2->SMPR2 = 0U;                   /* SMP1 = 000 → 3 周期 */

    ADC1->SR = 0U;
    ADC2->SR = 0U;
}

static void adc_tim_init(uint32_t arr)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0U;
    TIM2->PSC = 0U;                     /* 84 MHz 直接计数，32 位装得下 52500 */
    TIM2->ARR = arr;
    TIM2->CR2 = TIM_CR2_MMS_1;          /* MMS=010：update 事件作 TRGO */
    TIM2->EGR = TIM_EGR_UG;             /* 预装载 ARR/PSC */
    TIM2->SR = 0U;
}

static void adc_dma_init(uint16_t n)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;

    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0U) { /* 等流真正停下 */ }
    DMA2->LIFCR = DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0 | DMA_LIFCR_CTEIF0
                | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;

    DMA2_Stream0->PAR = (uint32_t)&ADC->CDR;
    DMA2_Stream0->M0AR = (uint32_t)adc_dma_buf;
    DMA2_Stream0->NDTR = 2U * n;
    /* CHSEL=0、DIR=00(外设→内存)、MSIZE=PSIZE=10(32bit)、MINC=1、
     * CIRC=1、HTIE=TCIE=1、PL=01(medium) */
    DMA2_Stream0->CR = DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1 | DMA_SxCR_MINC
                     | DMA_SxCR_CIRC | DMA_SxCR_HTIE | DMA_SxCR_TCIE
                     | DMA_SxCR_PL_0;
    DMA2_Stream0->FCR = 0U;             /* DMDIS=0：直接模式（外设/内存同为 32 位） */

    NVIC_SetPriority(DMA2_Stream0_IRQn, 1U);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

int adc_init(void)
{
    adc_gpio_init();
    adc_core_init();
    adc_tim_init(adc_bands[CFG_BAND_1KHZ].arr);
    adc_dma_init(adc_bands[CFG_BAND_1KHZ].n_fine);
    adc_n = adc_bands[CFG_BAND_1KHZ].n_fine;
    adc_fs = adc_fs_from_arr(adc_bands[CFG_BAND_1KHZ].arr);
    adc_arr = adc_bands[CFG_BAND_1KHZ].arr;

    /* F4 没有软件自校准位（D5）：ADON 已在 core_init 置 1，等 tSTAB ~3 µs */
    board_delay_us(3U);

    NVIC_SetPriority(ADC_IRQn, 0U);      /* AWD 优先级最高 */
    NVIC_EnableIRQ(ADC_IRQn);
    return 0;
}

void adc_awd_enable(int on)
{
    adc_awd_on = on ? 1 : 0;
    if (on) {
        ADC1->HTR = CFG_AWD_HTR;
        ADC1->LTR = CFG_AWD_LTR;
        ADC2->HTR = CFG_AWD_HTR;
        ADC2->LTR = CFG_AWD_LTR;
        /* AWDSGL=1 + AWDCH 选中各自的规则通道；AWDEN 规则组、AWDIE 中断 */
        ADC1->CR1 |= ADC_CR1_AWDEN | ADC_CR1_AWDIE | ADC_CR1_AWDSGL;
        ADC2->CR1 |= ADC_CR1_AWDEN | ADC_CR1_AWDIE | ADC_CR1_AWDSGL
                   | (1UL << ADC_CR1_AWDCH_Pos);
    } else {
        ADC1->CR1 &= ~(ADC_CR1_AWDEN | ADC_CR1_AWDIE);
        ADC2->CR1 &= ~(ADC_CR1_AWDEN | ADC_CR1_AWDIE);
        ADC1->SR = ~ADC_SR_AWD;
        ADC2->SR = ~ADC_SR_AWD;
    }
}

int adc_awd_tripped(void)
{
    return (adc_awd != 0U) ? 1 : 0;
}

uint32_t adc_ovr_count(void)
{
    return adc_ovr;
}

void adc_clear_errors(void)
{
    adc_ovr = 0U;
    adc_dmaerr = 0U;
    adc_awd = 0U;
    adc_ovr_streak = 0U;
    adc_need_reinit = 0;
}

void adc_stop(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    ADC1->CR2 &= ~ADC_CR2_ADON;
    ADC2->CR2 &= ~ADC_CR2_ADON;
    adc_running = 0;
}

static void adc_start(const acq_cfg_t *cfg)
{
    const uint32_t arr = (CFG_TIMCLK1_HZ / cfg->fs_hz) - 1U;

    adc_stop();
    adc_clear_errors();
    adc_consumed = 0U;
    adc_events = 0U;

    adc_n = cfg->n;
    adc_fs = cfg->fs_hz;
    adc_arr = arr;

    adc_tim_init(arr);
    adc_dma_init(cfg->n);
    adc_core_init();
    adc_awd_enable(adc_awd_on);
    board_delay_us(3U);                 /* tSTAB */

    DMA2_Stream0->CR |= DMA_SxCR_EN;
    TIM2->CR1 |= TIM_CR1_CEN;
    adc_running = 1;
}

/* ---- 采集 ---------------------------------------------------------------- */
static int adc_wait_block(uint32_t timeout_ms)
{
    const uint32_t start = board_millis();
    while (adc_events == adc_consumed) {
        if (adc_awd != 0U) {
            return ADC_ERR_AWD;
        }
        if (adc_running == 0) {
            return ADC_ERR_AWD;
        }
        if ((board_millis() - start) > timeout_ms) {
            adc_stop();
            return ADC_ERR_TIMEOUT;
        }
        board_iwdg_feed();
    }
    return ADC_OK;
}

int adc_acquire(const acq_cfg_t *cfg, adc_block_t *blk)
{
    if (!adc_cfg_valid(cfg) || (blk == 0)) {
        return ADC_ERR_CFG;
    }

    const uint32_t arr = (CFG_TIMCLK1_HZ / cfg->fs_hz) - 1U;
    if ((adc_running == 0) || (adc_need_reinit != 0) ||
        (arr != adc_arr) || (cfg->n != adc_n)) {
        adc_start(cfg);
    }

    /* 一块的时间 + 100 ms 余量；100 Hz 档 640 ms 是最长的一块 */
    const uint32_t block_ms = ((uint32_t)cfg->n * 1000U) / cfg->fs_hz;
    const uint32_t ts = board_millis();
    const int wr = adc_wait_block(block_ms + 100U);
    const uint32_t ovr_streak = adc_ovr_streak;   /* 立刻快照，避免被下一块 ISR 改掉 */
    if (wr != ADC_OK) {
        blk->n = 0U;
        blk->sat_count = 0U;
        blk->dma_err = 1U;
        return wr;
    }

    /* 偶数事件=半满=前半缓冲，奇数=全满=后半缓冲 */
    const uint32_t half = adc_consumed & 1U;
    const volatile uint32_t *src = &adc_dma_buf[half * cfg->n];
    uint16_t sat = 0U;
    for (uint16_t i = 0U; i < cfg->n; ++i) {
        const uint32_t w = src[i];
        blk->cdr[i] = w;
        const uint32_t lo = w & 0xFFFFU;
        const uint32_t hi = w >> 16;
        if ((lo <= CFG_ADC_CLIP_LO) || (lo >= CFG_ADC_CLIP_HI) ||
            (hi <= CFG_ADC_CLIP_LO) || (hi >= CFG_ADC_CLIP_HI)) {
            ++sat;
        }
    }
    adc_consumed++;

    blk->n = cfg->n;
    blk->s = cfg->s;
    blk->f0_hz = cfg->f0_hz;
    blk->fs_hz = cfg->fs_hz;
    blk->ts_ms = ts;
    blk->sat_count = sat;
    blk->dma_err = (uint16_t)((adc_dmaerr != 0U) ? 1U : 0U);

    if (ovr_streak != 0U) {
        /* OVR：本块作废，重新初始化 ADC；连续 3 次 → TIMEOUT（§4.4） */
        adc_need_reinit = 1;
        blk->n = 0U;
        return (ovr_streak >= 3U) ? ADC_ERR_TIMEOUT : ADC_ERR_OVR;
    }
    if (sat != 0U) {
        blk->n = 0U;
        return ADC_ERR_CLIP;     /* 削顶：交由上层降档重试，不直接出数 */
    }
    return ADC_OK;
}

int adc_acquire_avg(const acq_cfg_t *cfg, uint16_t blocks, adc_block_t *dst)
{
    if ((blocks == 0U) || (dst == 0)) {
        return ADC_ERR_CFG;
    }
    uint32_t sat_sum = 0U;
    uint32_t err_sum = 0U;
    int rc = ADC_OK;
    for (uint16_t k = 0U; k < blocks; ++k) {
        rc = adc_acquire(cfg, dst);
        if (rc != ADC_OK) {
            return rc;
        }
        sat_sum += dst->sat_count;
        err_sum += dst->dma_err;
    }
    dst->sat_count = (uint16_t)((sat_sum > 65535U) ? 65535U : sat_sum);
    dst->dma_err = (uint16_t)((err_sum > 65535U) ? 65535U : err_sum);
    return rc;
}

/* ---- ISR：只搬运与计数，不做计算（协作说明 §7）-------------------------- */
void DMA2_Stream0_IRQHandler(void)
{
    const uint32_t isr = DMA2->LISR;
    const uint32_t flags = isr & (DMA_LISR_HTIF0 | DMA_LISR_TCIF0 | DMA_LISR_TEIF0
                                | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0);
    if (flags == 0U) {
        return;
    }
    DMA2->LIFCR = flags;

    if ((flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) != 0U) {
        adc_dmaerr++;
    }
    if ((flags & (DMA_LISR_HTIF0 | DMA_LISR_TCIF0)) != 0U) {
        adc_events++;
        if (DMA2_Stream0->NDTR > (2U * adc_n)) {
            adc_dmaerr++;
        }
    }
    if (((ADC1->SR & ADC_SR_OVR) != 0U) || ((ADC2->SR & ADC_SR_OVR) != 0U)) {
        adc_ovr++;
        adc_ovr_streak++;
        ADC1->SR = ~ADC_SR_OVR;
        ADC2->SR = ~ADC_SR_OVR;
        adc_need_reinit = 1;
    } else {
        adc_ovr_streak = 0U;
    }
}

/* AWD：10 Ω 档削顶/短路的最后一道保险，响应 <10 µs（§4.4、R18）。
 * 只做三件事：静音 DDS → 停 TIM2/DMA → 置事件。 */
void ADC_IRQHandler(void)
{
    if (((ADC1->SR & ADC_SR_AWD) != 0U) || ((ADC2->SR & ADC_SR_AWD) != 0U)) {
        dds_hold_reset(1);
        TIM2->CR1 &= ~TIM_CR1_CEN;
        DMA2_Stream0->CR &= ~DMA_SxCR_EN;
        adc_running = 0;
        adc_awd++;
        ADC1->SR = ~ADC_SR_AWD;
        ADC2->SR = ~ADC_SR_AWD;
    }
}