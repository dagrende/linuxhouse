/* 
command in command prompt:
c - clear device table
s - search devices and stay in command line mode
l - enter read loop
r - send reset pulse and return presence (01 presence, 00 no presence)
ohh - send hex byte hh 
ihh - receive hex hh bytes
ti - input bytes forever
tohh - output byte hh forever

commands in readloop:
s <addr> <value>[ <propname>] - set property (no response or error response)
v - get version (response r avrow <version>)
a - get all present devices as e arr events
x - exit to command line mode
sp <propname> <value>
ap - get all property values as e pchange events

responses:
r <respone text>
err <message>

events:
e change <addr> <value>
e pchange <propname> <value>
e arr <addr>
e dep <addr>
*/

#include <io.h>
#include <interrupt.h>
#include <signal.h>
#include <progmem.h>
#include "drtype.h"
#include "uartio.h"
#include "numio.h"

#define clockFreq 8000000 
#define baudRate 19200
#define SEARCH_PERIOD_TICKS_INIT 61

#define delay31(ustime) \
                outp(1, TCCR0); outp(-ustime, TCNT0); outp((1 << TOV0), TIFR); \
                while ((inp(TIFR) & (1 << TOV0)) == 0) ; 
#define delay255(ustime) \
                outp(2, TCCR0); outp(-ustime, TCNT0); outp((1 << TOV0), TIFR); \
                while ((inp(TIFR) & (1 << TOV0)) == 0) ; 
#define delay16383(ustime) \
                outp(3, TCCR0); outp(-(ustime / 8), TCNT0); outp((1 << TOV0), TIFR); \
                while ((inp(TIFR) & (1 << TOV0)) == 0) ; 

#define HMAX 1    // hysteres window size
#define DEVICE_FLAG_HPOS_MASK 1     // value position in hysteres window
#define DEVICE_FLAG_FOUND_MASK 2     // this device is found or created in this search operation
#define MISSING_COUNT_MAX 4

typedef struct {
    u08 address[8];
    u16 value;
    u08 sampleTime;
    u08 missingCount;       // 0 at arr, inc-ed each dep until MISSING_COUNT_MAX
    u08 flags;
} Device;

// forward declarations
void initIo(void);
u08 resetOW(void);      // returns 1 if presense pulse
u08 inputOWBit(void);
void outputOWBit(u08 bit);
u08 inputOWByte(void);
void outputOWByte(u08 b);
void searchDevices(void);
boolean traceBits(u08 from);
boolean retraceBits(u08 to);
void clearDeviceFlags(void);
void storeDevice(u08 *address);
void clearDeviceFlags(void);
void printArrDepEvent(u08 isArrival, u08 i);
void removeMissingDevices(void);
void readLoop(void);
boolean readDeviceType10(Device *devicep);
boolean readDeviceType05(Device *devicep);
boolean outputOWByte3(u08 b);
boolean outputOWAddress3(u08 *address);
void outputOWAddress(u08 *address);
u08 getTime(void);
void printAddr(u08 *addr);
void sendAllParams(void);
void processSetParamCmd(void);
u08 getchToCr(void);


// PORTC bits
#define OWI 0   // one wire level input
#define OWO 1   // one wire level output
#define OWZ 2   // one wire output buffer gate (1 is high z)
#define PC_READ_SYNC 3  // oscilloscope sync bit for ow reads
#define PC_WRITE_SYNC 4  // oscilloscope sync bit for ow writes
#define PC_INBUF_OVF 5
#define PC_READ_PRESENSE 6


#define showProgress 0

u08 a[8];
u08 b[64];
u08 bn;
Device deviceTable[20];
u08 deviceCount = 0;
#define NO_DEVICE_INDEX 0xff
u08 searchPeriodTicks = SEARCH_PERIOD_TICKS_INIT;
u08 crc;
char paramNames[] __attribute__ ((progmem)) = 
        "rstper\0srchper\0devcnt";
enum {rstper = 0, srchper, devcnt};
u08 params[3] = {100, SEARCH_PERIOD_TICKS_INIT, 101};

// returns 1 if null terminated strings ramStr equal to progStr, 0 else
u08 eqpstr(char* ramStr, char* pstr) {
    char ch;

    while (*ramStr++ == (ch = PRG_RDB(pstr++))) {
        if (ch == 0) {
            return 1;   // equal
        }
    }
    return 0;   // not equal
}

// return index of string ramstr in pstr that contains a sequence
// of null terminated strings. pstr is pstrend - pstr bytes long 
//including last null.
u08 strIndexInPstr(char* ramstr, char* pstr,  u08 pstrlen) {
    char *s = ramstr;
    u08 i = 0;
    char ch;
    char* pstrend = pstr + pstrlen;
    
    while (pstr < pstrend) {
        if (*s++ == (ch = PRG_RDB(pstr++))) {
            if (ch == 0) {
                break;
            }
        } else {
            i++;
            s = ramstr; // start over
            // skip to next pstring
            while (PRG_RDB(pstr++) != 0) {
            }
        }
    }
    return i;
}

void sendAllParams() {
    u08 i = 0;
    char* pstr = paramNames;

    while (pstr < (paramNames + sizeof paramNames)) {
        PRINT("e pchange ");  putps(pstr); PRINT(" "); printfu08(params[i]); eol();
        i++;
        // skip to next pstring
        while (PRG_RDB(pstr++) != 0) {
        }
    }
}

void processSetParamCmd() {
    u08 ch, i;

    ch = getchToCr();        // skip space
    
    // get param name into b
    i = 0;
    ch = getchToCr();
    while (i < sizeof b - 1 && !(ch == '\r' || ch == ' ')) {
        b[i++] = ch;
        ch = getchToCr();
    }
    b[i++] = 0;
    
    // find paramName index
    i = strIndexInPstr(b, paramNames, sizeof paramNames);
    if (i < (sizeof params / sizeof params[0])) {
        params[i] = receiveHexInt8();
        PRINT("e pchange ");  puts(b); PRINT(" "); printfu08(params[i]); eol();
    }
}

// make crc calculation step with bit 0 of bit0
void crcCalc(u08 bit0) {
    u08 t = crc;
    crc >>= 1;
    if (((t ^ bit0) & 1) == 1) {
        crc ^= 0x8c;
    }
}

u08 findDeviceIndexByAddress(u08 *address) {
    u08 i;

    for (i = 0; i < deviceCount; i++) {
        if (memcmp(&deviceTable[i], address, 8) == 0) {
            return i;
        }
    }
    return NO_DEVICE_INDEX;
}

Device *findDeviceByAddress(u08 *address) {
    u08 i = findDeviceIndexByAddress(address);
    if (i != NO_DEVICE_INDEX) {
        return &deviceTable[i];
    }
    return null;
}

// get next char.
// if it is  cr, return cr and unget the cr.
u08 getchToCr(void) {
    u08 ch = getch();
    if (ch == '\r') {
        ungetch(ch);
    }
    return ch;
}

// read past next cr.
// next getch will return first char on next line.
void skipCr(void) {
    u08 ch;
    do {
        ch = getch();
    } while (ch != '\r');
}

boolean processCommand(void) {
    u08 ch, i, deviceType, value;
    Device *devicep;

    do { // skip previous linefeed
        ch = getchToCr();
    } while (ch == '\n');

    if (ch == 's') {    //--------- set command
        ch = getchToCr();
        if (ch == 'p') {
            processSetParamCmd();
        } else {
    
            // receive device address
            clearNumErr();
            for (i = 0; i < 8; i++) {
                a[i] = receiveHexInt8();
            }
            if (isNumErr()) {
                // addr is not 8 hex digits
                PRINT("err device address error"); eol();
                skipCr();
                return true;
            }
            // find device addres in table
            devicep = findDeviceByAddress(a);
            if (devicep == null) {  
                // addr not found in table
                PRINT("err unknown device"); eol();
                skipCr();
                return true;
            }
            deviceType = a[0];      // first addr byte is device type
    
            // get value
            getchToCr();        // skip space
            value = receiveHexInt8();
            
            // get property name
            do {        // skip space
                ch = getchToCr();
            } while (ch == ' ');
    
            i = 0;
            while (i < sizeof b - 1 && !(ch == '\r' || ch == ' ')) {
                b[i++] = ch;
                ch = getchToCr();
            }
            b[i++] = 0;
            
            // use commands according to device type
            if (deviceType == 0x05) {
                if ((value ^ ((u08)devicep->value)) & 1) {  // if state not equal
                    // toggle state
                    resetOW();
                    outputOWByte(0x55);     // match rom
                    outputOWAddress(a);
                }
            } else if (deviceType == 0x47) {
                u08 cmd;
                if (b[0] == 0 || b[0] == 'v') {     // if value - sets only output bits
                    cmd = 0x4e;
                } else if (b[0] == 'd') {   // if dir
                    cmd = 0x35;
                } else if (b[0] == 'p') {   // if port -  set both out bits and in bits (pull up)
                    cmd = 0x37;
                } else {
                    PRINT("err unknown property"); eol();
                    skipCr();
                    return true;
                }
                resetOW();
                outputOWByte(0x55);         // match rom
                outputOWAddress(a);
                outputOWByte(cmd);
                outputOWByte(value);
            }
        }
    } else if (ch == 'v') {	//-------- version command
        PRINT("r avrow 1.1");   // reply with name and version
        eol();
    } else if (ch == 'x') {	//--------- exit command (from loop mode)
        return false;
    } else if (ch == 'a') {
        ch = getchToCr();
        if (ch == 'p') {
            sendAllParams();	//--------- send all parameter values 
        } else {
            deviceCount = 0;	//--------- resend arrival events 
        }
    }
    skipCr();
    return true;
}

int main(void) {
    initIo();
    deviceCount = 0;

    readLoop();
    PRINT("avrow>");
    for (;;) {
        u08 ch, val;

        while (1) {
            ch = getch();
            if (!(ch == ' ' || ch == '\r' || ch == '\n')) {
                break;
            }
        }

        if (ch == 's') {        // s - search
            searchDevices();
        } else if (ch == 'c') { // c - clear device tabe
            deviceCount = 0;
        } else if (ch == 'i') { // ixx - input xx bytes
            u08 cnt = receiveHexInt8();
            if (!isNumErr()) {
                while (cnt-- > 0) {
                    printfu08(inputOWByte());
                }
            }
        } else if (ch == 'o') { // oxx - output byte xx
            // fixme output until not hex digit
            outputOWByte(receiveHexInt8());
        } else if (ch == 'r') { // r - reset
            printfu08(resetOW());
        } else if (ch == 'l') { // l - loop reading all devices in deviceTable
            readLoop();
        } else if (ch == 'h') { // h - help
            PRINT("ixx - input xx bytes"); eol();
            PRINT("oxx... - output byte xx"); eol();
            PRINT("r - reset and return presense"); eol();
            PRINT("s - search all devices"); eol();
            PRINT("c - clear device table"); eol();
            PRINT("ti - input bytes forever"); eol();
            PRINT("toxx - output byte xx forever"); eol();
            PRINT("tt - output 16 bit timer"); eol();
        } else if (ch == 't') { // t - test
            ch = getch();
            if (ch == 'i') {    // ti - input bytes forever
                for (;;) {
                    inputOWByte();
                }
            } else if (ch == 'o') {     // toxx - output byte xx forever
                val = receiveHexInt8();
                for (;;) {
                    outputOWByte(val);
                }
            } else if (ch == '0') {
                PRINT("0"); outputOWBit(0);
            } else if (ch == '1') {
                PRINT("1"); outputOWBit(1);
            } else if (ch == 'b') {
                PRINT("b"); 
                if (inputOWBit()) {
                    PRINT("1");
                } else {
                    PRINT("0");
                }
            } else if (ch == 'q') {
                // fixme output until not hex digit
                u08 n = receiveHexInt8();
                PRINT(" "); printfu08(n);
            } else if (ch == 't') {     // tt - output timer register
                val = inp(TCNT1L);
                printfu08(inp(TCNT1H));
                printfu08(val);
            }
        }
    }
}

boolean readDeviceType10(Device *devicep) {
    boolean changed = false;
    u16 value;
    u08 j, valueCrc;
    s08 hpos;
    u08 scratchPad[9];
    u08 time = getTime();
    u08 lapse = time - devicep->sampleTime;
    if (lapse >= 15) {// more than 500 mS since latest convert t command
        // read sensor
        if (resetOW()) {
            outputOWByte(0x55);
            outputOWAddress(devicep->address);
            outputOWByte(0xbe); // read scratchpad command
            crc = 0;
            for (j = 0; j < 9; j++) {
                scratchPad[j] = inputOWByte();
            }
            valueCrc = crc;

            // send convert t command
            devicep->sampleTime = getTime();
            resetOW();
            outputOWByte(0x55);
            outputOWAddress(devicep->address);
            outputOWByte(0x44);

            // determine if indicate changed with hysteresis
            if (valueCrc == 0) {
                value = (((u16)scratchPad[1]) << 8) + scratchPad[0];
                hpos = devicep->flags & DEVICE_FLAG_HPOS_MASK;
                hpos += (s16)value - devicep->value;
                if (hpos > HMAX) {
                    hpos = HMAX;
                    changed = true;
                }
                if (hpos < 0) {
                    hpos = 0;
                    changed = true;
                }
                devicep->flags &= ~DEVICE_FLAG_HPOS_MASK;
                devicep->flags |= hpos;
    
                devicep->value = value;
            } else {
                PRINT("err crc r "); printAddr(devicep->address); eol();
            }
        }
    }
    return changed;
}

boolean readDeviceType05(Device *devicep) {
    u08 value;
    boolean changed = false;
    
    if (resetOW()) {
        devicep->sampleTime = getTime();
        outputOWByte(0xf0);
        if (outputOWAddress3(devicep->address)) {
            value = inputOWByte();
            changed = value != (u08)devicep->value;
            devicep->value = (u16)value;
        }
    }
    return changed;
}

boolean readDeviceType47(Device *devicep) {
    u08 value, valueCrc;
    boolean changed = false;
    
    if (resetOW()) {
        devicep->sampleTime = getTime();
        outputOWByte(0x55);
        outputOWAddress(devicep->address);
        outputOWByte(0xbe); // read scratchpad command
        crc = 0;
        value = inputOWByte();
        valueCrc = inputOWByte();
        if (crc != 0) {
            // crc error - forget this reading and indicate no change
            PRINT("err crc r "); printAddr(devicep->address); PRINT(" "); 
            printfu08(value); printfu08(valueCrc); eol();
            return false;
        }
        changed = value != (u08)devicep->value;
        devicep->value = (u16)value;
    }
    return changed;
}

// 33 mS per tick
u08 getTime() {
    inp(TCNT1L);
    return inp(TCNT1H);
}

void readLoop() {
    u08 deviceType, i;
    Device *devicep;
    boolean changed = false;
    u08 lastSearchTime;     // getTime() at last search

    lastSearchTime = getTime();
    searchDevices();

    while (true) {
        // search for devices every searchPeriodTicks - about every 2th second
        u08 time = getTime();
        if ((u08)(lastSearchTime + searchPeriodTicks - time) > 127) {
            searchPeriodTicks = SEARCH_PERIOD_TICKS_INIT;
            searchDevices();
            lastSearchTime += searchPeriodTicks;
        }

        // if a line is received - excecute it
        if (lineAvail() > 0) {
            if (!processCommand()) {
                goto endReadLoop;  // received exit command - terminate loop
            }
        }

        for (i = 0; i < deviceCount; i++) {
            devicep = &deviceTable[i];
            deviceType = devicep->address[0];
            if (deviceType == 0x10) {
                changed = readDeviceType10(devicep);
            } else if (deviceType == 0x05) {
                changed = readDeviceType05(devicep);
            } else if (deviceType == 0x47) {
                changed = readDeviceType47(devicep);
            } else {
                PRINT("err unknown device type: "); 
                printfu08(deviceType);
                eol();
            }
            delay16383(200);
            if (changed) {
                PRINT("e change ");
                printAddr(devicep->address);
                PRINT(" ");
                printfu16(devicep->value);
                eol();
            }

            // if a line is received - excecute it
            if (lineAvail() > 0) {
                if (!processCommand()) {
                    goto endReadLoop;  // received exit command - terminate loop
                }
            }
        }
    }
endReadLoop:
}

void clearDeviceFlags() {
    u08 i;

    for (i = 0; i < deviceCount; i++) {
        deviceTable[i].flags &= ~DEVICE_FLAG_FOUND_MASK;     // clear flag
    }
}

void printArrDepEvent(u08 isArrival, u08 i) {
    if (isArrival) {
        PRINT("e arr ");
    } else {
        PRINT("e dep ");
    }
    printAddr(deviceTable[i].address);
    eol();
}

void printAddr(u08 *addr) {
    u08 i;
    for (i = 0; i < 8; i++) {
        printfu08(*addr++);
    }
}

void crcCalcByte(u08 aByte) {
    u08 i;
    for (i = 0; i < 8; i++) {
        crcCalc(aByte);
        aByte >>= 1;
    }
}

void crcCalcAddr(u08 *address) {
    u08 i;
    for (i = 0; i < 8; i++) {
        crcCalcByte(*address++);
    }
}

void storeDevice(u08 *address) {
    Device *devicep;

    crc = 0;
    crcCalcAddr(address);
    if (crc != 0) {
        PRINT("err crc search"); eol();
        return;
    }

    devicep = findDeviceByAddress(address);
    if (devicep != null) {
        // device aleady present in table
        devicep->flags |= DEVICE_FLAG_FOUND_MASK;     // set flag
        devicep->missingCount = 0;      // device is not missing
        return;
    }
    // add new device entry in table
    devicep = &deviceTable[deviceCount];
    memcpy(devicep->address, address, 8);
    devicep->value = 4711;      // unlikely value to get intital change event
    devicep->flags = DEVICE_FLAG_FOUND_MASK;  // set this flag only
    devicep->missingCount = 0;      // device is not missing
    
    printArrDepEvent(true, deviceCount);
    deviceCount++;
}

// remove devices that was not found in the search
// but they must be missing MISSING_COUNT_MAX searches to be removed
void removeMissingDevices() {
    u08 i, j;

    j = 0;
    for (i = 0; i < deviceCount; i++) {
        Device *devicep = &deviceTable[i];
        if (devicep->flags & DEVICE_FLAG_FOUND_MASK) {
            // this device is present
            // move this and following device entries up over any removed ones
            if (j < i) {
                memcpy(&deviceTable[j], devicep, 
                        sizeof deviceTable[0]);
            }
            j++;
        } else {
            // this device is missing
            if (devicep->missingCount > MISSING_COUNT_MAX) {
                // device missing max times - remove entry and send event
                printArrDepEvent(false, i);
            } else {
                // not missing enough times - keep entry and count its missingness
                // and decrease retry time
                searchPeriodTicks = 8;  // try next time in about 264 mS
                devicep->missingCount++;
                j++;
            }
        }
    }
    deviceCount = j;
}

void searchDevices() {
    u08 br;

    bn = 0;
    clearDeviceFlags();

    if (resetOW()) {
        outputOWByte(0xf0); // search command
        if (traceBits(0)) {
            while (1) {
                storeDevice(a);
        
                if (bn == 0)
                    break;	// all devices found
        
                // go back to last branch (conflicting bit), and follow the 1 route
                bn--;
                br = b[bn];
        
                a[br / 8] |= (1 << (br & 7));	// switch route by setting bit br 
        
                resetOW();
                outputOWByte(0xf0); // search command
                if (!retraceBits(br + 1)) {
                    break;
                }
                if (!traceBits(br + 1)) {
                    break;
                }
            }
        }
    }
    removeMissingDevices();
}

// Trace all bits from from to 64. Save in a the bits found.
// If conflicting bits, the pos is pushed on b (incrementing bn) and
// the 0 route is followed.
// return 0 if OK, 1 if any error
boolean traceBits(u08 from) {
    u08 twoBits, i, bit;

    for (i = from; i < 64; i++) {
        // get the two bits
        twoBits = 0;
        if (inputOWBit())
            twoBits |= 1;		// non inverted bit is 1
        if (inputOWBit())
            twoBits |= 2;		// inverted bit is 1

        // interpret result
        if (twoBits == 0) {                     // some devices has 0 and some has 1
            a[i / 8] &= ~(1 << (i & 7));        // turn left this time
            b[bn++] = i;                        // save branch bit index
        } else if (twoBits == 1) {
            a[i / 8] |= (1 << (i & 7));	        // all devices has 1
        } else if (twoBits == 2) {
            a[i / 8] &= ~(1 << (i & 7));        // all devices has 0
        } else {
            PRINT("err traceBits: device address error, no bus answer"); eol();
            return false;
        }

        // send chosen bit
        bit = a[i / 8] & (1 << (i & 7));
        outputOWBit(bit);
    }
    return true;
}

// trace all bits from 0 up to to (not including).
// the route in a is followed even if conflicting bits.
boolean retraceBits(u08 to) {
    u08 twoBits, i;

    for (i = 0; i < to; i++) {
        // get the two bits
        twoBits = 0;
        if (inputOWBit())
            twoBits |= 1;		// non inverted bit is 1
        if (inputOWBit())
            twoBits |= 2;		// inverted bit is 1

        if (twoBits == 3) {		// if no device still with us
            PRINT("err retraceBits: no device answers"); eol();
            return false;
        }

        // send chosen bit
        if (a[i / 8] & (1 << (i & 7)))
            outputOWBit(1);
        else
            outputOWBit(0);
    }
    return true;
}

// sends reset pulse, waits and read presense pulse and returns
// 1 if device present, 0 if no device present
u08 resetOW(void) {
    u08 presense;
    // time 480uS
    cbi(PORTC, OWO);            // will pull down
    cbi(PORTC, OWZ);            // turn on one wire output
    cbi(PORTC, PC_READ_PRESENSE);
    delay16383(480);
    sbi(PORTC, OWZ);            // turn off one wire output, to let it up again
    delay255(74);               // between 60 and 75uS
    // read presense pulse
    presense = bit_is_clear(PINC, OWI);
    sbi(PORTC, PC_READ_PRESENSE);
    delay16383(450);
    return presense;
}

// inputs a bit from OW and return 0 or 1
u08 inputOWBit() {
    u08 bit;

    cli();
    sbi(PORTC, PC_READ_SYNC);
    cbi(PORTC, PC_READ_SYNC);
    cbi(PORTC, OWO);            // pull down
    cbi(PORTC, OWZ);            // turn on one wire output
    delay31(40);
    sbi(PORTC, OWZ);            // turn off one wire output, to let it up again
    delay255(9);
    bit = bit_is_set(PINC, OWI);        // sample ow input
    sei();
    delay255(100);
    delay255(100);
    crcCalc(bit);
    return bit;
}

// inputs a byte with bit0 coming first
u08 inputOWByte() {
    u08 b = 0;
    int i;

    for (i = 0; i < 8; i++) {
        b >>= 1;
        if (inputOWBit()) {
            b |= 0x80;
        }
    }
    return b;
}

// outputs a one bit if bit != 0, otherwise a zero bit
void outputOWBit(u08 bit) {
    cli();
    sbi(PORTC, PC_WRITE_SYNC);
    cbi(PORTC, PC_WRITE_SYNC);
    if (bit != 0) {
        // 1 - a 3uS pulse
        cbi(PORTC, OWO);        // will pull down
        cbi(PORTC, OWZ);        // turn on one wire output
        delay31(40);            // 5 uS
        sbi(PORTC, OWZ);        // turn off one wire output, to let it up again
        delay255(60);
        crcCalc(1);
    } else {
        // 0 - a 60uS pulse
        cbi(PORTC, OWO);        // will pull down
        cbi(PORTC, OWZ);        // turn on one wire output
        delay255(60);
        sbi(PORTC, OWZ);        // turn off one wire output, to let it up again
        delay255(5);
        crcCalc(0);
    }
    sei();
    delay255(100);
    delay255(100);
}

void outputOWByte(u08 b) {
    int i;

    for (i = 0; i < 8; i++) {
        outputOWBit(b & 1);
        b >>= 1;
    }
}

boolean outputOWByte3(u08 b) {
    u08 i;

    for (i = 0; i < 8; i++) {
        if (inputOWBit() & inputOWBit()) {
            return false;       // device not present
        }
        outputOWBit(b & 1);
        b >>= 1;
    }
    return true;
}

void outputOWAddress(u08 *address) {
    int i;

    for (i = 0; i < 8; i++) {
        outputOWByte(address[i]);
    }
}

boolean outputOWAddress3(u08 *address) {
    int i;

    for (i = 0; i < 8; i++) {
        if (!outputOWByte3(address[i])) {
            return false;
        }
    }
    return true;
}

// initialize io ports and devices
void initIo(void) {
    /* Initialise UART */
    uartInit(RATE_FACTOR(clockFreq, baudRate));
    sbi(DDRD, 1);  // make serial linetxd an output
    
    
    // set up timer 0 to generate overflow interrupt 
    outp(0, TCNT0);     // reset TCNT0
    outp(0, TCCR0);     // stop the counter

    // set up timer 1 to count time
    outp(5, TCCR1B);    // count by CK/1024

    // make PC0 input, PC1 and PC2 outputs, and PC3 read sync out, PC4 write sync out
    outp(0xfe, DDRC);
    sbi(PORTC, OWZ);    // set ow output to high Z
}

// signal handler for tcnt0 overflow interrupt
SIGNAL(SIG_OVERFLOW0) {
}


// utility routines


