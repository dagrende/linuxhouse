// numeric io library
// uses uartio

#include <io.h>
#include <interrupt.h>
#include <signal.h>
#include <progmem.h>
#include "uartio.h"

u08 isNumberFormatErr;

u08 isNumErr(void) {
    return isNumberFormatErr;
}

void clearNumErr(void) {
    isNumberFormatErr = 0;
}

void printfu04(u08 data) {
    // Send 4-bit hex value
    u08 ch = data & 0x0f;
    if (ch > 9) {
        ch += 'A' - 10;
    } else {
        ch += '0';
    }
    putch(ch);
}

void printfu08(u08 data) {
    /* Send 8-bit hex value */
    printfu04(data >> 4);
    printfu04(data);
}

void printfu16(u16 data) {
    /* Send 16-bit hex value */
    printfu08(data >> 8);
    printfu08(data);
}

// print 32 bit hex word to serial port
void printfu32(int32_t i) {
    /* Send 32-bit hex value */
    printfu16(i >> 16);
    printfu16(i);
}

u08 isHexDigit(u08 d) {
    return ('0' <= d && d <= '9') || ('A' <= d && d <= 'F') || ('a' <= d && d <= 'f');
}

// receive one hex digit from serial port
u08 receiveHexDigit(void) {
    u08 d;

    isNumberFormatErr = 0;
    d = getch();
    if (isHexDigit(d)) {
        d -= '0';
        if (d > 9) {
            d -= 'A' - '9' - 1;
        }
        if (d > 15) {
            d -= 'a' - 'A';
        }
        return d;
    } else {
        isNumberFormatErr = 1;
        ungetch(d);
        return 0;
    }
}

// receive hex byte from serial port
u08 receiveHexInt8(void) {
    u08 i;
    i = receiveHexDigit();
    i = (i << 4) + receiveHexDigit();
    return i;
}

// receive 16 bit hex word from serial port
int32_t receiveHexInt16(void) {
    int32_t i;
    i = (u08)receiveHexInt8();
    i = i * 256 + (u08)receiveHexInt8();
    return i;
}

// receive 32 bit hex word from serial port
int32_t receiveHexInt32(void) {
    int32_t i;
    i = (u08)receiveHexInt8();
    i = i * 256 + (u08)receiveHexInt8();
    i = i * 256 + (u08)receiveHexInt8();
    i = i * 256 + (u08)receiveHexInt8();
    return i;
}


