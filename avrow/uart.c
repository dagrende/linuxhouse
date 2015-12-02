#include <io.h>
#include <interrupt.h>
#include <signal.h>
#include <progmem.h>
#include "uart.h"

/* UART global variables */
volatile u08   UART_Ready;
volatile u08   UART_ReceivedChar;
         u08   UART_RxChar;
         u08*  pUART_Buffer;

/* end-of-line string = CR LF */
prog_char UART_pszEndOfLine[3] = {0x0d,0x0a,0};

/* UART Transmit Complete Interrupt Function */
SIGNAL(SIG_UART_TRANS)      
{
    /* Test if a string is being sent */
    if (pUART_Buffer!=0)
    {
        /* Go to next character in string */
        pUART_Buffer++;
        /* Test if the end of string has been reached */
        if (PRG_RDB(pUART_Buffer)==0)
        {
            /* String has been sent */
            pUART_Buffer = 0;
            /* Indicate that the UART is now ready to send */
            UART_Ready   = 1;
            return;
        }
        /* Send next character in string */
        outp( PRG_RDB(pUART_Buffer), UDR );
        return;
    }
    /* Indicate that the UART is now ready to send */
    UART_Ready = 1;
}

/* UART Receive Complete Interrupt Function */
SIGNAL(SIG_UART_RECV)      
{
    /* Indicate that the UART has received a character */
    UART_ReceivedChar = 1;
    /* Store received character */
    UART_RxChar = inp(UDR);
}

void UART_SendByte(u08 Data)
{   
    /* wait for UART to become available */
    while(!UART_Ready);
    UART_Ready = 0;
    /* Send character */
    outp( Data, UDR );
}

u08 UART_ReceiveByte(void)
{
    /* wait for UART to indicate that a character has been received */
    while(!UART_ReceivedChar);
    UART_ReceivedChar = 0;
    /* read byte from UART data buffer */
    return UART_RxChar;
}

u08 UART_ReceiveChar(void) {
  return UART_ReceiveByte() & 0x7f;   // return with parity bit masked off
}

void UART_PrintfProgStr(u08* pBuf)
{
    /* wait for UART to become available */
    while(!UART_Ready);
    UART_Ready = 0;
    /* Indicate to ISR the string to be sent */
    pUART_Buffer = pBuf;
    /* Send first character */
    outp( PRG_RDB(pUART_Buffer), UDR );
}

void UART_PrintfEndOfLine(void)
{
    /* wait for UART to become available */
    while(!UART_Ready);
    UART_Ready = 0;
    /* Indicate to ISR the string to be sent */
    pUART_Buffer = UART_pszEndOfLine;
    /* Send first character */
    outp( PRG_RDB(pUART_Buffer), UDR );
}

void UART_PrintfU4(u08 Data)
{
    /* Send 4-bit hex value */
    u08 Character = Data&0x0f;
    if (Character>9)
    {
        Character+='A'-10;
    }
    else
    {
        Character+='0';
    }
    UART_SendByte(Character);
}

void UART_Printfu08(u08 Data)
{
    /* Send 8-bit hex value */
    UART_PrintfU4(Data>>4);
    UART_PrintfU4(Data   );
}

void UART_Printfu16(u16 Data)
{
    /* Send 16-bit hex value */
    UART_PrintfU4(Data>>12);
    UART_PrintfU4(Data>> 8);
    UART_PrintfU4(Data>> 4);
    UART_PrintfU4(Data    );
}

void UART_Init_internal(u08 theBaudRateFactor)
{
    UART_Ready        = 1;
    UART_ReceivedChar = 0;
    pUART_Buffer      = 0;

    /* enable RxD/TxD and interrupts */
    outp(BV(RXCIE)|BV(TXCIE)|BV(RXEN)|BV(TXEN), UCR);

    /* set baud rate */
    outp( (u08)theBaudRateFactor, UBRR);  
    /* enable interrupts */
    sei();
}
