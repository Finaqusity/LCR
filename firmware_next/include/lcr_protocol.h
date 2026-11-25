#ifndef LCR_PROTOCOL_H
#define LCR_PROTOCOL_H

#include <stdint.h>

#include "lcr_app.h"

/* ASCII line protocol v1 for the web console (docs/rewrite/网页上位机需求与接口.md).
 * Owns the USART1 command line and mirrors application events as frames:
 *   $BOOT  at startup / on '?'
 *   $RES   after every completed measurement
 *   $STATE whenever running/mode/model/calibration state changes
 *   $CAL   while a calibration sweep advances, and once when it ends
 *   $MSG   for operator messages shown on the LCD (CAL SAVED, ...)
 *   $ERR / $ACK for command results
 * All calls are non-blocking; poll from the main loop. */
void lcr_protocol_init(lcr_app *app, const char *fw_version, uint32_t board_id,
                       uint32_t fixture_id);
void lcr_protocol_boot(lcr_app *app);
void lcr_protocol_poll(lcr_app *app, uint32_t now);

#endif
