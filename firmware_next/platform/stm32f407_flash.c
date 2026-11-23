#include "lcr_board.h"
#include "lcr_journal.h"
#include "stm32f4xx.h"

#define FLASH_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR)
static uint32_t address(unsigned bank) { return 0x08040000u + bank * 0x20000u; }
static int idle(void) {
    for (unsigned i = 0u; i < 50000000u; ++i)
        if (!(FLASH->SR & FLASH_SR_BSY)) return !(FLASH->SR & FLASH_ERRORS);
    return 0;
}
static int unlock(void) {
    if (!lcr_board_muted() || (TIM2->CR1 & TIM_CR1_CEN) || (DMA2_Stream0->CR & DMA_SxCR_EN)) return 0;
    if (!idle()) { FLASH->SR = FLASH_ERRORS | FLASH_SR_EOP; if (!idle()) return 0; }
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123u; FLASH->KEYR = 0xcdef89abu;
    }
    FLASH->SR = FLASH_ERRORS | FLASH_SR_EOP;
    return !(FLASH->CR & FLASH_CR_LOCK);
}
static void invalidate_data_cache(void) {
    const uint32_t acr = FLASH->ACR;
    FLASH->ACR = acr & ~FLASH_ACR_DCEN;
    FLASH->ACR |= FLASH_ACR_DCRST;
    FLASH->ACR &= ~FLASH_ACR_DCRST;
    FLASH->ACR = acr;
    __DSB(); __ISB();
}
static int read_bank(void *context, unsigned bank, size_t offset, uint8_t *bytes, size_t count) {
    (void)context;
    if (bank > 1u || offset > 0x20000u || count > 0x20000u - offset || !bytes) return 0;
    const volatile uint8_t *source = (const volatile uint8_t *)(address(bank) + offset);
    for (size_t i = 0u; i < count; ++i) bytes[i] = source[i];
    return 1;
}
static int erase_bank(void *context, unsigned bank) {
    (void)context;
    if (bank > 1u || !unlock()) return 0;
    const uint32_t saved = __get_PRIMASK(); __disable_irq();
    FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_SER | ((bank + 6u) << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    const int ok = idle();
    FLASH->CR = FLASH_CR_LOCK;
    invalidate_data_cache();
    __set_PRIMASK(saved);
    return ok;
}
static int program_word(void *context, unsigned bank, size_t offset, uint32_t value) {
    (void)context;
    if (bank > 1u || (offset & 3u) || offset > 0x20000u - 4u || !unlock()) return 0;
    volatile uint32_t *destination = (volatile uint32_t *)(address(bank) + offset);
    if ((*destination & value) != value) { FLASH->CR = FLASH_CR_LOCK; return 0; }
    const uint32_t saved = __get_PRIMASK(); __disable_irq();
    FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
    *destination = value;
    const int ok = idle();
    FLASH->CR = FLASH_CR_LOCK;
    invalidate_data_cache();
    __set_PRIMASK(saved);
    return ok && *destination == value;
}
lcr_journal_io lcr_board_journal(void) {
    const lcr_journal_io io = {0, read_bank, erase_bank, program_word};
    return io;
}
uint32_t lcr_board_identity(void) {
    const volatile uint32_t *uid = (const volatile uint32_t *)0x1fff7a10u;
    uint32_t id = uid[0] ^ (uid[1] * 16777619u) ^ (uid[2] * 2166136261u);
    return id ? id : 1u;
}
