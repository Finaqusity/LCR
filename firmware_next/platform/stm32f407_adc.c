#include "lcr_board.h"
#include "lcr_core.h"
#include "stm32f4xx.h"

/* RM0090: dual regular simultaneous, DMA access mode 2, TIM2_TRGO. */
static uint32_t dma_words[1200] __attribute__((section(".dma_buffer"), aligned(4)));
static uint16_t cpu_pairs[2400];
static volatile int completed;
static size_t expected_pairs;
static uint32_t capture_token;

void lcr_board_capture_cancel(void *context) {
    (void)context;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;
    TIM2->CR1 = 0u;
    NVIC_DisableIRQ(DMA2_Stream0_IRQn);
    /* No acquisition may survive a cancellation or buffer ownership transfer. */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    for (unsigned timeout = 0u; (DMA2_Stream0->CR & DMA_SxCR_EN) && timeout < 100000u; ++timeout) __NOP();
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN;
    (void)RCC->APB2ENR;
    ADC1->CR2 = 0u; ADC2->CR2 = 0u;
    DMA2->LIFCR = 0x3du;
    NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    completed = 0;
    __DSB();
}

int lcr_board_capture_start(void *context, unsigned frequency, unsigned range, size_t count, uint32_t token) {
    (void)context; (void)range;
    if (frequency >= 4u || count != lcr_frequencies[frequency].sample_pairs || count > 1200u || !token) return 0;
    lcr_board_capture_cancel(0);
    if (DMA2_Stream0->CR & DMA_SxCR_EN) return 0;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->MODER |= 15u;
    GPIOA->PUPDR &= ~15u;
    RCC->APB2RSTR |= RCC_APB2RSTR_ADCRST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_ADCRST;
    /* ADCPRE=/4 (21MHz), MULTI=00110; DDS=0: finite DMA block. */
    ADC->CCR = ADC_CCR_ADCPRE_0 | ADC_CCR_DMA_1 | ADC_CCR_MULTI_1 | ADC_CCR_MULTI_2;
    ADC1->CR1 = 0u; ADC2->CR1 = 0u;
    ADC1->SMPR2 = 0u; ADC2->SMPR2 = 0u;
    ADC1->SQR1 = 0u; ADC2->SQR1 = 0u;
    ADC1->SQR3 = 0u; ADC2->SQR3 = 1u;
    TIM2->CR1 = 0u; TIM2->CR2 = 0u;
    TIM2->PSC = 0u;
    TIM2->ARR = lcr_frequencies[frequency].timer_divisor - 1u;
    TIM2->CNT = 0u; TIM2->EGR = TIM_EGR_UG; TIM2->SR = 0u;
    TIM2->CR2 = TIM_CR2_MMS_1;
    DMA2_Stream0->CR = DMA_SxCR_MINC | DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1 |
                          DMA_SxCR_PL | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    DMA2_Stream0->FCR = DMA_SxFCR_FEIE;
    DMA2_Stream0->PAR = (uint32_t)&ADC->CDR;
    DMA2_Stream0->M0AR = (uint32_t)dma_words;
    DMA2_Stream0->NDTR = (uint32_t)count;
    expected_pairs = count; capture_token = token; completed = 0;
    ADC2->CR2 = ADC_CR2_ADON;
    ADC1->CR2 = ADC_CR2_ADON | (6u << ADC_CR2_EXTSEL_Pos);
    for (unsigned i = 0u; i < 1000u; ++i) __NOP(); /* ADC power-up stabilization. */
    ADC1->SR = 0u; ADC2->SR = 0u;
    NVIC_SetPriority(DMA2_Stream0_IRQn, 0u);
    NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    DMA2_Stream0->CR |= DMA_SxCR_EN;
    ADC1->CR2 |= ADC_CR2_EXTEN_0;
    __DSB();
    TIM2->CR1 = TIM_CR1_CEN;
    return 1;
}

void DMA2_Stream0_IRQHandler(void) {
    const uint32_t status = DMA2->LISR;
    if (!(status & 0x3du)) return;
    TIM2->CR1 = 0u;
    ADC1->CR2 &= ~ADC_CR2_EXTEN;
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    const int bad = (status & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) ||
                     (ADC1->SR & ADC_SR_OVR) || (ADC2->SR & ADC_SR_OVR) || DMA2_Stream0->NDTR;
    completed = (status & DMA_LISR_TCIF0) && !bad ? 1 : -1;
    DMA2->LIFCR = 0x3du;
    __DSB();
}

int lcr_board_capture_take(const uint16_t **pairs, size_t *count, uint32_t *token) {
    if (!pairs || !count || !token || !completed) return 0;
    const int result = completed;
    NVIC_DisableIRQ(DMA2_Stream0_IRQn);
    TIM2->CR1 = 0u;
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    for (unsigned timeout = 0u; (DMA2_Stream0->CR & DMA_SxCR_EN) && timeout < 100000u; ++timeout) __NOP();
    if (DMA2_Stream0->CR & DMA_SxCR_EN) return -1;
    __DMB();
    completed = 0;
    if (result < 0) return -1;
    for (size_t i = 0u; i < expected_pairs; ++i) {
        const uint32_t word = dma_words[i];
        cpu_pairs[2u * i] = (uint16_t)word;
        cpu_pairs[2u * i + 1u] = (uint16_t)(word >> 16);
    }
    *pairs = cpu_pairs; *count = expected_pairs; *token = capture_token;
    return 1;
}
