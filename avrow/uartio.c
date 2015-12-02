// buffered uart io library
// receive by interrupt into buffer with xon/xoff control
// send synchronously

#include <io.h>
#include <interrupt.h>
#include <signal.h>
#include <progmem.h>
#include "drtype.h"
#include "uartio.h"

unsigned char inbuf[100];
u08 inbufPutPos = 0;       // put next received char here in inbuf
u08 inbufGetPos = 0;       // get next char here in inbuf
volatile u08 inbufCount = 0;     // nbr of chars in inbuf
volatile u08 inbufLineCount = 0;   // number of complete lines in inbuf (lines terminated with cr)
u08 isStopped = 0;
#define LOW_LEVEL_PERCENT 40
#define HIGH_LEVEL_PERCENT 85

void uartInit(u08 baudRateFactor) {
    inbufPutPos = 0;
    inbufGetPos = 0;
    inbufCount = 0;
    inbufLineCount = 0;

    /* enable RxD/TxD and receive interrupt */
    outp(BV(RXCIE)|BV(RXEN)|BV(TXEN), UCR);

    /* set baud rate */
    outp( (u08)baudRateFactor, UBRR);  
    /* enable interrupts */
    sei();
}

SIGNAL(SIG_UART_RECV) {
    u08 ch = inp(UDR) & 0x7f;
/*    if (ch == 'S' - ' ' || ch == 'Q' - ' ') {
        return;
    }*/
    // check for buffer overflow
    if (inbufCount >= sizeof inbuf) {
        sbi(PORTC, 5);
        // remove first line in inbuf, or all characters if no \r 
        // by advancing inbufGetPos up to and including the first \r 
        while (inbufCount > 0) {
            inbufCount--;
            if (inbuf[inbufGetPos++] == '\r') {
                break;
            }
        }
    }
    inbuf[inbufPutPos++] = ch;
    if (inbufPutPos >= sizeof inbuf) {
        inbufPutPos = 0;
    }
    inbufCount++;
    if (ch == '\r') {
        inbufLineCount++;
    }
/*    if (!isStopped && inbufCount > HIGH_LEVEL_PERCENT * sizeof inbuf / 100) {
        // fixme may cause errors in other calls to putc
        putch('S' - ' ');
        isStopped = true;
    }
*/
}

u08 getch(void) {
    u08 ch;

    while (inbufCount == 0) 
        ;
    ch = inbuf[inbufGetPos++];
    inbufCount--;
    if (inbufGetPos >= sizeof inbuf) {
        inbufGetPos = 0;
    }
    if (ch == '\r') {
        inbufLineCount--;
    }
    if (isStopped && inbufCount < LOW_LEVEL_PERCENT * sizeof inbuf / 100) {
        putch('Q' - ' ');
        isStopped = false;
    }
cbi(PORTC, 5);
    return ch;
}

void ungetch(u08 ch) {
    inbuf[--inbufGetPos] = ch;
    inbufCount++;
    if (ch == '\r') {
        inbufLineCount++;
    }
}

u08 charAvail(void) {
    return inbufCount;
}

u08 lineAvail(void) {
    return inbufLineCount;
}

void putch(u08 ch) {
    loop_until_bit_is_set(USR, UDRE);    // use TXC to let getch send C-S before next statement
    outp(ch, UDR);
}

void puts(u08 *s) {
    while (*s) {
        putch(*s++);
    }
}

void putps(u08 *s) {
    while (PRG_RDB(s)) {
        putch(PRG_RDB(s++));
    }
}

void eol(void) {
    putch(0x0d);
    putch(0x0a);
}
