#ifndef __UART_H__
#define __UART_H__

#include "drtypes.h"

/* UART Baud rate calculation */
#define UART_Init(clockFreq, baudRate) UART_Init_internal(clockFreq / (baudRate * 16l) - 1)

/* Global functions */
extern void UART_SendByte(u08 Data);
extern u08  UART_ReceiveByte(void);
extern u08 UART_ReceiveChar(void);
extern void UART_PrintfProgStr(u08* pBuf);
extern void UART_PrintfEndOfLine(void);
extern void UART_Printfu08(u08 Data);
extern void UART_Printfu16(u16 Data);
extern void UART_Init_internal(u08 theBaudSelect);

/* Macros */
#define PRINT(string) (UART_PrintfProgStr(PSTR(string)))
#define EOL           UART_PrintfEndOfLine

#endif

