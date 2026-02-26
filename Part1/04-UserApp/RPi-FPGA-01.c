// RPi-FPGA-01-FIFO - Raspberry Pi 4 and Lattice ice40HX4K FPGA
// Copyright (C) 2026  David Dommett, email: david.dommett@gmail.com 

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

//#include "gpio.h"
//#include "fpgaConfig.h"
#include <errno.h>
#include <fcntl.h>   // open mmap
#include <sys/mman.h>

#define	INPUT  0
#define	OUTPUT 1

int GpioSetup(void);
void GpioSetPinInOut(int pin, int mode);
void FpgaConfigInitialize(void);
void uwait_barrier_sync(void);
void FpgaConfigPowerUp(void);
void FlashWriteEnable(void);
void FlashWrite(int addr, char *data, int n);
void FlashRead(int addr, char *data, int n);
void FlashWait(void);
void FlashWriteSector(int addr, char *data, int size);
int fpgaConfigProgram(char *filename);

#define CFG_SS   25 // PIN 22
#define CFG_SCK  19 // PIN 35
#define CFG_SI   16 // PIN 36
#define CFG_SO   21 // PIN 40
#define CFG_RST  24 // PIN 18
#define CFG_DONE 26 // PIN 37

#define CFG_RST_BIT ((int)0x01 << CFG_RST)
#define CFG_SS_BIT  ((int)0x01 << CFG_SS)
#define CFG_SI_BIT  ((int)0x01 << CFG_SI)
#define CFG_SCK_BIT ((int)0x01 << CFG_SCK)
#define CFG_SO_BIT  ((int)0x01 << CFG_SO)
#define CFG_DONE_BIT  ((int)0x01 << CFG_DONE)

// gpio points to the base of gpio memory (32-bit registers)
volatile unsigned int *gpio;
volatile unsigned int *gpio_set_high;
volatile unsigned int *gpio_set_low;
volatile unsigned int *gpio_read;

#define	GPIO_PERI_BASE_2711 0xFE000000

//***********************************************************************************
// On a PI4, a BCM2711 has 58 gpio, bank 0 contains gpio 0..27 (which is all that is
// relevant to us).
//
// The registers, offset from the base:
//
// 0x00 GPFSEL0 GPIO Function Select 0  Function (In/Out/etc.) pins 9..0
// 0x04 GPFSEL1 GPIO Function Select 1  19..10
// 0x08 GPFSEL2 GPIO Function Select 2  29..20
// 0x0c GPFSEL3 GPIO Function Select 3  39..30
// 0x10 GPFSEL4 GPIO Function Select 4  49..40
// 0x14 GPFSEL5 GPIO Function Select 5  59..50
// 0x18 
// 0x1c GPSET0 GPIO Pin Output Set 0    Set output high pins 31..0
// 0x20 GPSET1 GPIO Pin Output Set 1    Set output high pins 57..32
// 0x24 
// 0x28 GPCLR0 GPIO Pin Output Clear 0  Set output low pins 31..0
// 0x2c GPCLR1 GPIO Pin Output Clear 1  Set output low pins 57..32
// 0x30 
// 0x34 GPLEV0 GPIO Pin Level 0         Read input pins 31..0
// 0x38 GPLEV1 GPIO Pin Level 1         Read input pins 57..32
//***********************************************************************************

//***********************************************************************************
// Setup gpio.
// Map gpio memory/registers into our address space. We use /dev/gpiomem at offset
// 0x00200000. This will allow us to read/write BCM2711 registers directly.
//***********************************************************************************
int GpioSetup(void)
{
    int fd = open ("/dev/gpiomem", O_RDWR | O_SYNC | O_CLOEXEC); 
    if (fd == -1)
    {
        printf("ERROR: Cannot open /dev/gpiomem");
        exit(EXIT_FAILURE);
    }

    int BLOCK_SIZE = (4*1024);
    gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x00200000);
    if (gpio == MAP_FAILED)
    {
        printf("ERROR: Cannot mmap gpio registers");
        exit(EXIT_FAILURE);
    }

    gpio_set_high = gpio + 7;
    gpio_set_low = gpio + 10;
    gpio_read = gpio + 13;

    return 0 ;
}

//***********************************************************************************
// Set pin to be input or output.
// This sets the relevant register to a 3-bit pattern defining the pin as input or
// output. '000' is input, '001' is output
//***********************************************************************************
void GpioSetPinInOut(int pin, int mode)
{
    int fSel;  // The offset from base address to select the function for the pin
    int shift; // The amount to shift over for the pin
    volatile unsigned int *reg;

    fSel = pin / 10;
    shift = pin - (fSel * 10);
    shift *= 3;

    reg = gpio + fSel; 	// Point to the correct register
    *reg &= ~(7 << shift);  // Zero the relevant 3 bits in the register (make input)
    if (mode == OUTPUT)
        *reg |= (1 << shift); // Set mode to output 
}

//***********************************************************************************
// Initialize the fpgaConfig module
// - Setup the 6 CFG lines
//***********************************************************************************
void FpgaConfigInitialize(void)
{
    GpioSetPinInOut(CFG_SS,   OUTPUT);
    GpioSetPinInOut(CFG_SCK,  OUTPUT);
    GpioSetPinInOut(CFG_SI,   INPUT);
    GpioSetPinInOut(CFG_SO,   OUTPUT);
    *gpio_set_high = CFG_RST_BIT;  // Make sure we don't get a low-going pulse glitch
    GpioSetPinInOut(CFG_RST,  OUTPUT);
    GpioSetPinInOut(CFG_DONE, INPUT);

    *gpio_set_high = CFG_SS_BIT;
    *gpio_set_low = CFG_SCK_BIT;
    *gpio_set_low = CFG_SO_BIT;
    *gpio_set_high = CFG_RST_BIT;
}

//***********************************************************************************
// Sync Memory
//***********************************************************************************
void uwait_barrier_sync(void)
{
    int k;
    for (k = 0; k < 10; k++)
        asm volatile("" : : : "memory");
    __sync_synchronize();
}

//***********************************************************************************
// Send a byte out bit-banged SPI, receive byte simultaneously 
//***********************************************************************************
uint32_t SpiSendReceive(uint32_t data, int nbits)
{
    uint32_t rdata = 0;
    int i;

    for (i = nbits-1; i >= 0; i--)
    {
        uwait_barrier_sync();
        if (data & (1 << i))
        {
            *gpio_set_high = CFG_SO_BIT;
        }
        else
        {
            *gpio_set_low = CFG_SO_BIT;
        }

        uwait_barrier_sync();
        if (*gpio_read & CFG_SI_BIT)
            rdata |= 1 << i;

        uwait_barrier_sync();
        *gpio_set_high = CFG_SCK_BIT;

        uwait_barrier_sync();
        *gpio_set_low = CFG_SCK_BIT;
    }

    return rdata;
}

//***********************************************************************************
// Enable Flash Chip
//***********************************************************************************
void FpgaConfigPowerUp()
{
    *gpio_set_low = CFG_SS_BIT;
    SpiSendReceive(0xAB, 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Enable writing to flash
//***********************************************************************************
void FlashWriteEnable()
{
    *gpio_set_low = CFG_SS_BIT;
    SpiSendReceive(0x06, 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Write a byte 
//***********************************************************************************
void FlashWrite(int addr, char *data, int n)
{
    *gpio_set_low = CFG_SS_BIT;
    SpiSendReceive(0x02, 8);
    SpiSendReceive(addr, 24);
    while (n--)
        SpiSendReceive(*(data++), 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Read a byte 
//***********************************************************************************
void FlashRead(int addr, char *data, int n)
{
    *gpio_set_low = CFG_SS_BIT;
    SpiSendReceive(0x03, 8);
    SpiSendReceive(addr, 24);
    while (n--)
        *(data++) = SpiSendReceive(0, 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Wait on flash
//***********************************************************************************
void FlashWait()
{
    while (1)
    {
        *gpio_set_low = CFG_SS_BIT;
        SpiSendReceive(0x05, 8);
        int status = SpiSendReceive(0, 8);
        *gpio_set_high = CFG_SS_BIT;

        if ((status & 0x01) == 0)
            break;

        usleep(1000);
    }
}

//***********************************************************************************
// Write a sector to flash
//***********************************************************************************
void FlashWriteSector(int addr, char *data, int size)
{
    char buffer[256];

    printf("Writing 0x%06x .. 0x%06x\n", addr, addr+size-1);
    fflush(stdout);

    // Erase a 64k block in flash
    FlashWriteEnable();
    *gpio_set_low = CFG_SS_BIT;
    SpiSendReceive(0xd8, 8);
    SpiSendReceive(addr, 24);
    *gpio_set_high = CFG_SS_BIT;
    FlashWait();

    for (int i = 0; i < size; i += 256)
    {
        FlashWriteEnable();
        FlashWrite(addr+i, data+i, size-i < 256 ? size-i : 256);
        FlashWait();
    }

    // Read back memory to verify
    for (int i = 0; i < size; i += 256)
    {
        FlashRead(addr+i, buffer, size-i < 256 ? size-i : 256);

        if (memcmp(buffer, data+i, size-i < 256 ? size-i : 256)) 
        {
            printf("ERROR\n");
            fflush(stdout);
            return;
        }
    }
}

//***********************************************************************************
// Write a bitstream file into the Flash and program the FPGA
//***********************************************************************************
int FpgaConfigProgram(char *filename)
{
    *gpio_set_low = CFG_RST_BIT;

    int addr = 0;
    int size = 0;
    char buffer[64*1024];

    FILE *f = fopen(filename, "rb");

    if (f == NULL) 
    {
        printf("Open failed: %s: %s\n", filename, strerror(errno));
        return 1;
    }

    do 
    {
        addr += size;
        size = 0;

        while (size < (int)sizeof(buffer)) 
        {
            int rc = fread(buffer+size, 1, sizeof(buffer)-size, f);
            if (rc <= 0) break;
            size += rc;
        }

        if (size > 0)
            FlashWriteSector(addr, buffer, size);
    } while (size == sizeof(buffer));

    *gpio_set_low = CFG_RST_BIT;
    usleep(2000);

    *gpio_set_high = CFG_RST_BIT;
    usleep(500000);

    if ((*gpio_read & CFG_DONE_BIT) == 0)
    {
        printf("Config FPGA failed!\n");
        return 1;
    }

    printf("Config success.\n");
    return 0;
}


// This is a design which uses a 64 deep Async FIFO - 9 bits wide

// Define signals on GPIO pins
#define D0       0  
#define D1       1  
#define D2       2  
#define D3       3  
#define D4       4  
#define D5       5 
#define D6       6 
#define D7       7 
#define D8       8 
#define D9       9 
#define D10      10 
#define D11      11 
#define D12      12
#define D13      13
#define D14      14
#define D15      15
//               16 // Used by fpgaConfig
#define FRAME_DONE 17 // PIN 11
// Unused        18
//               19 // Used by fpgaConfig
// SPI1_MOSI     20
//               21 // Used by fpgaConfig
#define R_EN     22 // PIN 15
#define FRAME    23 // PIN 16
//               24 // Used by fpgaConfig
//               25 // Used by fpgaConfig
//               26 // Used by fpgaConfig
#define EXT_RESET_N 27 // PIN 13
                       //
#define EXT_RESET_N_BIT  ((int)0x01 << EXT_RESET_N)
#define FRAME_DONE_BIT  ((int)0x01 << FRAME_DONE)
#define FRAME_BIT  ((int)0x01 << FRAME)
#define R_EN_BIT  ((int)0x01 << R_EN)

#define READ_HOLD 10

static unsigned char buf[64];
static unsigned int bufInt[64];
int R_EN_state = 0;
static unsigned int lastHead = 0;
static unsigned int errors = 0;
static unsigned int misses = 0;
static unsigned int totalGood = 0;
static int rec1[1000];
static int rec2[1000];
static int rec3[1000];
static int recCount = 0;
static int readCount = 0;

//***********************************************************************************
// This function will read in a buffer (currently 64 x 8-bit bytes).
// Returns: 0 if nothing to read.
//          -1 if read less than a full buffer.
//          -2 if read exceeds buffer size.
//          -5 data in footer.
//          -11 if read less than a full buffer (in footer).
//          -12 if read exceeds buffer size (in footer).
//***********************************************************************************
int ReadBuffer(void)
{
    int count = -1;
    int footer = 0;

    // If FRAME is high - there is data to read, if not, return
    if (!(*gpio_read & FRAME_BIT))
        return 0;

    // Buffer is just 64 bytes for right now
    while (1)
    {
        count++;
        if (count >= 64)
        {
            // We got to the end of our array before getting the footer?
            if (footer)
                return -12;
            return -2;
        }
        bufInt[count] = *gpio_read;
        buf[count] = bufInt[count];
        if (!(bufInt[count] & FRAME_BIT))
        {
            // Frame dropped - no more data to read
            if (footer)
                return -11;
            return -1;
        } 

        // If the Raspberry Pi runs too fast, the FPGA does not switch the data fast
        // enough after getting a read pulse, at 8 writes in the following, it works,
        // but anything less, and we sometimes get bad data - the FRAME pulse is 8.4us
        // when it is bad and 9.1us (or more) for good (this is with an Async FIFO)
        // This is true when using an Async FIFO or a simple memory

        // Tell the FPGA we have done a read
        if (R_EN_state == 0)
            R_EN_state = 1;
        else
            R_EN_state = 0;
        if (R_EN_state)
        {
            for (int i=0; i<READ_HOLD; i++)
                *gpio_set_high = R_EN_BIT;
        }
        else
        {
            for (int i=0; i<READ_HOLD; i++)
                *gpio_set_low = R_EN_BIT;
        }

        if (footer)
        {
            // We have already hit the footer (end of packet)
            if (!(bufInt[count] & 0x100)) // Bit D8
            {
                // We read data before end of footer?
                return -5;
            }                
            // Check for end of footer
            if (buf[count] == 0xFF)
            {
                break;
            }
        }
        else
        {
            // Look for footer
            footer = bufInt[count] & 0x100; // Bit D8
        }
    }

    return 64;
}

void loop3(void) 
{
    int bytesRead;
    unsigned int curHead;
    int wrap;

    *gpio_set_low = EXT_RESET_N_BIT; 
    usleep(1000);
    *gpio_set_high = EXT_RESET_N_BIT; 

    clock_t startTime, endTime;
    startTime = clock();

    while (1)
    {
        *gpio_set_high = CFG_SO_BIT;
        bytesRead = ReadBuffer();
        if (bytesRead == 0)
           continue; 
        
        readCount++;
        curHead = buf[2] * 256 + buf[1];

        // Check for wrap-around every 1.3 seconds
        wrap = 0;
        if ((curHead == 0) && (lastHead == 65535))
            wrap = 1;

        if (bytesRead < 0)
        {
            //pr_err("ReadBuffer error %d, %d\n", bytesRead, lastHead);
            if (recCount < 1000)
            {
                rec1[recCount] = bytesRead;
                rec2[recCount] = lastHead;
                rec3[recCount] = curHead;
                recCount++;
            }
            errors++;
        }
        else if ((curHead != (lastHead+1)) && (!wrap))
        {
            if (recCount < 1000)
            {
                rec1[recCount] = readCount;
                rec2[recCount] = lastHead;
                rec3[recCount] = curHead;
                recCount++;
                float f = recCount;
                f /= 100;
                int recCount2 = recCount;
                recCount2 /= 100;
                f *= 100;
                recCount2 *= 100;
                int recCount3 = f;
                if (recCount2 == recCount3)
                {
                    printf("recCount=%d, readCount=%d, misses=%d, errors=%d\n", recCount, readCount, misses, errors);
                }
                if (recCount >= 1000)
                {
                    endTime = clock();
                    double total_t = (double)(endTime - startTime) / CLOCKS_PER_SEC;
                    printf("1000 records in %f seconds.\n", total_t); 
                    FILE *f = fopen("temp.txt", "w");
                    for (int j=0; j<1000; j++)
                        fprintf(f, "%d: 0x%04X 0x%04X\n", rec1[j], rec2[j], rec3[j]);
                    fclose(f);
                }
            }
            if (curHead > lastHead)
            {
                misses += (curHead-lastHead+1);
            }
            else
            {
                misses += (curHead + 65535) - lastHead + 1;
            }
        }
        else
        {
            totalGood++;
        }
        if (bytesRead == 64)
        {
        }

        lastHead = curHead;        
        *gpio_set_low = CFG_SO_BIT;
    }

    *gpio_set_low = CFG_SO_BIT;
}

int main(int argc, char **argv)
{
	if (argc != 2 || (argv[1][0] == '-' && argv[1][1]) || !argv[1][0]) {
		printf("Usage: %s PROG       ... program FPGA\n", argv[0]);
		printf("Usage: %s LOOP       ... loop forever\n", argv[0]);
		return 1;
	}

	printf("START\n");
	GpioSetup();
	FpgaConfigInitialize();

    //---------- PROG ----------
	if (strcmp(argv[1], "PROG") == 0)
    {
		FpgaConfigPowerUp();
        printf("PROGRAMMING FPGA\n");
        FpgaConfigProgram("../03-RPi-FPGA-01-FIFO/RPi-FPGA-01.bin");
        return 0;
    }
 
    *gpio_set_low = R_EN_BIT;
    *gpio_set_low = EXT_RESET_N_BIT;
    *gpio_set_low = FRAME_DONE_BIT;

	GpioSetPinInOut(R_EN, OUTPUT);
	GpioSetPinInOut(EXT_RESET_N, OUTPUT);
	GpioSetPinInOut(FRAME_DONE, OUTPUT);

	GpioSetPinInOut(FRAME, INPUT);

	GpioSetPinInOut(D0, INPUT);
	GpioSetPinInOut(D1, INPUT);
	GpioSetPinInOut(D2, INPUT);
	GpioSetPinInOut(D3, INPUT);
	GpioSetPinInOut(D4, INPUT);
	GpioSetPinInOut(D5, INPUT);
	GpioSetPinInOut(D6, INPUT);
	GpioSetPinInOut(D7, INPUT);
	GpioSetPinInOut(D8, INPUT);

	// Verify that the FPGA is configured
	if (!(*gpio_read & CFG_DONE_BIT)) 
	{
		printf("CFG_DONE is 0 - FPGA not programmed\n");
		printf("EXITING.\n");
		return 0;
	}

    //---------- LOOP ----------
	if (strcmp(argv[1], "LOOP") == 0)
    {
        printf("LOOP FOREVER\n");
        loop3();
        return 0;
    }
 
	printf("DONE.\n");
	return 0;
}

