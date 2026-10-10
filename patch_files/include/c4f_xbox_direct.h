/* Control4Free USB-direct research extension -- Xbox 360 wired only.
 * Copyright (c) 2026. This adaptation is distributed under GPL-3.0-or-later,
 * following Control4Free's GPL-3.0 license.
 * USB work runs on a dedicated thread, never on the pad/game thread.
 */
#ifndef C4F_XBOX_DIRECT_H
#define C4F_XBOX_DIRECT_H

#include <stdint.h>
#include "c4f_sce.h"

/* 1 = Xbox 360 VID/PID opened and input thread running; 0 = unavailable. */
int c4fXboxDirectStart(void);
void c4fXboxDirectStop(void);
/* Copies latest valid report. Returns 1 for fresh data or 0 for neutral/stale.
 * 0 never means that the virtual controller should be disconnected. */
int c4fXboxDirectSample(ScePadData *out, uint32_t *sequence);
/* 1 = USB connection active; 0 = awaiting cable reconnect. No USB I/O here. */
int c4fXboxDirectConnected(void);

#endif
