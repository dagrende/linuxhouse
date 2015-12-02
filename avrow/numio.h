#ifndef __NUMIO_H__
#define __NUMIO_H__

#include "drtype.h"
#include <inttypes.h>

void printfu04(u08 Data);
void printfu08(u08 Data);
void printfu16(u16 data);
void printfu32(int32_t i);

u08 isNumErr(void);
u08 receiveHexDigit(void);
int8_t receiveHexInt8(void);
int32_t receiveHexInt16(void);

#endif

