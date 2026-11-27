#ifndef LCR_BOARD_H
#define LCR_BOARD_H

#include "lcr_frontend.h"
#include <stddef.h>
#include "lcr_journal.h"

/* STM32F407 platform. Requires reset startup to call this module's SystemInit. */
int lcr_board_init(void);
uint32_t lcr_board_millis(void);
int lcr_board_puts(const char *text);
int lcr_board_getchar(void); /* -1: empty; -2: UART receive error. Poll frequently. */
void lcr_board_led(int on);
unsigned lcr_board_buttons(void); /* Single key bit 0: 1 = WK_UP/PA0 pressed (active high). */
int lcr_board_dds_frequency(uint32_t hz); /* Accepted only while already muted. */
lcr_frontend_io lcr_board_frontend_io(void);
int lcr_board_capture_start(void *context, unsigned frequency, unsigned range, size_t count, uint32_t token);
void lcr_board_capture_cancel(void *context);
int lcr_board_capture_take(const uint16_t **pairs, size_t *count, uint32_t *token);
int lcr_board_display_init(void);
int lcr_board_display(const uint8_t *logical_pixels);
int lcr_board_muted(void);
lcr_journal_io lcr_board_journal(void);
uint32_t lcr_board_identity(void);

#endif
