/* 宿主测试专用 CMSIS 垫片（仅 firmware/test/host/，不参与 ARM 构建）。
 * 只提供 cal.c/meas.c 编译所需的寄存器结构体/宏/内联函数占位；
 * 真正的硬件函数在链接时被 -dead_strip 丢弃，宿主只跑纯算法。 */
#ifndef HOST_STM32F4XX_SHIM_H
#define HOST_STM32F4XX_SHIM_H

#include <stdint.h>

typedef struct { volatile uint32_t CR, SR, KEYR; } FLASH_TypeDef;
typedef struct { volatile uint32_t CYCCNT, CR, CTRL; } DWT_TypeDef;
typedef struct { volatile uint32_t KR, PR, RLR, SR; } IWDG_TypeDef;
typedef struct { volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2]; } GPIO_TypeDef;
typedef struct { volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR; } TIM_TypeDef;
typedef struct { volatile uint32_t SR, CR1, CR2, SMPR1, SMPR2, JOFR1, HTR, LTR, SQR1, SQR2, SQR3, JSQR, JDR1, DR, CSR, CCR, CDR; } ADC_TypeDef;
typedef struct { volatile uint32_t CR, NDTR, PAR, M0AR, M1AR, FCR; } DMA_Stream_TypeDef;

static FLASH_TypeDef  host_flash;
static DWT_TypeDef    host_dwt;
static IWDG_TypeDef   host_iwdg;
#define FLASH (&host_flash)
#define DWT   (&host_dwt)
#define IWDG  (&host_iwdg)

#define FLASH_CR_LOCK    (1U << 31)
#define FLASH_CR_PG      (1U << 0)
#define FLASH_CR_SER     (1U << 1)
#define FLASH_CR_MER     (1U << 2)
#define FLASH_CR_STRT    (1U << 16)
#define FLASH_CR_PSIZE   (3U << 8)
#define FLASH_CR_PSIZE_1 (1U << 9)
#define FLASH_CR_SNB     (15U << 3)
#define FLASH_SR_BSY     (1U << 16)
#define FLASH_SR_EOP     (1U << 0)
#define FLASH_SR_OPERR   (1U << 1)
#define FLASH_SR_WRPERR  (1U << 4)
#define FLASH_SR_PGAERR  (1U << 5)
#define FLASH_SR_PGPERR  (1U << 6)
#define FLASH_SR_PGSERR  (1U << 7)

static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __set_PRIMASK(uint32_t v) { (void)v; }
static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}
static inline void __NOP(void) {}
static inline void __DSB(void) {}
static inline void __ISB(void) {}

#endif