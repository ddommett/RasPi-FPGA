// fast_gpio_bus - Raspberry Pi 4 and Lattice ice40HX4K FPGA
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

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/vmalloc.h>

#include "../include/fast_gpio_bus.h"

#define GPIO_BASE_PHYS 0xFE200000 // GPIO base for PI 4
#define GPIO_SIZE      0xB4
 
//***********************************************************************************
// Note: This driver is hard-coded to work ONLY with a Raspberry Pi 4
//       and uses the fast_gpio_bus_overlay for the device tree.
//       It uses ALL GPIO pins - no other driver can use ANY of the GPIO pins.
//***********************************************************************************

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
// Function Prototypes
//***********************************************************************************
static int InitRingBuffer(unsigned long int capacity);
static void DeleteRingBuffer(void);
static int RingBufferWrite(struct BufferStruct *p);
static int RingBufferRead(struct BufferStruct *p);
static int Probe(struct platform_device *pdev);
static void Remove(struct platform_device *pdev);
static int kthreadFunction(void *data);
static void ReleaseResources(void);
void ResetCommunications(void);
int ReadBuffer(void);

static void GpioSetPinInOut(int pin, int mode);
static void FpgaConfigInitialize(void);
static void FpgaConfigPowerUp(void);
static void FlashWriteEnable(void);
static void FlashWrite(int addr, char *data, int n);
static void FlashRead(int addr, char *data, int n);
static void FlashWait(void);
static void FlashSector(int addr, char *data, int size);
static int FpgaConfigProgram(char *filename);
static void uwait_barrier_sync(void);
static uint32_t SpiSendReceive(uint32_t data, int nbits);

//***********************************************************************************
// Global variables
//***********************************************************************************
// To achieve maximum performance, we do not use gpiod (or any other gpio library),
// we manipulate the BCM2711 GPIO registers directly
// gpio points to the base of gpio memory (32-bit registers)
volatile unsigned int __iomem *gpio;
volatile unsigned int __iomem *gpio_set_high;
volatile unsigned int __iomem *gpio_set_low;
volatile unsigned int __iomem *gpio_read;

static char fpgaConfigBuffer[64*1024]; // Declared global as it exceeds stack frame size 

static struct hrtimer hrtimerReadData;
static struct task_struct *threadStartTimer;

static int R_EN_state = 0;
static unsigned char buf[64];
static unsigned int bufInt[64];
static unsigned int lastHead = 0;
static unsigned int errors = 0;
static unsigned int misses = 0;
static unsigned int totalGood = 0;
static int rec1[1000];
static int rec2[1000];
static int rec3[1000];
static int recCount = 0;
static int readCount = 0;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Dommett");
MODULE_DESCRIPTION("A LKM to communicate with a FPGA via the GPIO in a custom parallel (high-speed) manner.");

// 20uSec
#define HRTIMER_TIME_NS 20000

// How many times we should write the R_EN bit to be successful with a read
#define READ_HOLD 10

#define	INPUT  0
#define	OUTPUT 1

// Define data signals on GPIO pins
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

// Define control signals on GPIO pins
#define FRAME_DONE 17 // PIN 11
// Unused        18
// SPI1_MOSI     20
#define R_EN     22 // PIN 15
#define FRAME    23 // PIN 16
#define EXT_RESET_N 27 // PIN 13

// Define FPGA config signals on GPIO pins
#define CFG_SI   16 // PIN 36
#define CFG_SCK  19 // PIN 35
#define CFG_SO   21 // PIN 40
#define CFG_RST  24 // PIN 18
#define CFG_SS   25 // PIN 22
#define CFG_DONE 26 // PIN 37
                    
#define EXT_RESET_N 27 // PIN 13
                       
#define CFG_RST_BIT ((int)0x01 << CFG_RST)
#define CFG_SS_BIT  ((int)0x01 << CFG_SS)
#define CFG_SI_BIT  ((int)0x01 << CFG_SI)
#define CFG_SCK_BIT ((int)0x01 << CFG_SCK)
#define CFG_SO_BIT  ((int)0x01 << CFG_SO)
#define CFG_DONE_BIT  ((int)0x01 << CFG_DONE)

#define FRAME_DONE_BIT   ((int)0x01 << FRAME_DONE)
#define R_EN_BIT         ((int)0x01 << R_EN)
#define FRAME_BIT        ((int)0x01 << FRAME)
#define EXT_RESET_N_BIT  ((int)0x01 << EXT_RESET_N)

#define CHAR_SIZE (sizeof(char))

// Define a ring buffer to hold packets retrieved from the FPGA until they are read 
// by the userspace app
struct RingBuffer 
{
    struct BufferStruct *start;
    struct BufferStruct *end;
    struct BufferStruct *readptr;
    struct BufferStruct *writeptr;
};
struct RingBuffer rb;

//***********************************************************************************
// Initialize the ring buffer - allocate memory on the heap
//***********************************************************************************
static int InitRingBuffer(unsigned long int capacity)
{
    rb.start = NULL;

    char *mem = vmalloc(capacity * sizeof(struct BufferStruct));
    if (mem == NULL) 
    {
        return -1;
    }

    rb.start = (struct BufferStruct *)mem;
    rb.end = rb.start + capacity;
    rb.readptr = rb.start;
    rb.writeptr = rb.start;

    return 0;
}

//***********************************************************************************
// Destroy the ring buffer and free memory 
//***********************************************************************************
static void DeleteRingBuffer(void)
{
    if (rb.start == NULL)
        return;
    vfree(rb.start);
    rb.start = NULL;
}

//***********************************************************************************
// Try to write to the ring buffer
//***********************************************************************************
static int RingBufferWrite(struct BufferStruct *p)
{
    if (rb.start == NULL)
        return RINGBUFFER_ERR_NULL;

    if ((rb.writeptr + 1) == rb.readptr)
        return RINGBUFFER_ERR_FULL;

    memcpy(rb.writeptr, p, sizeof(struct BufferStruct));
    if ((rb.writeptr+1) >= rb.end) 
        rb.writeptr = rb.start;
    else
        rb.writeptr += 1;

    return RINGBUFFER_OK;
}

//***********************************************************************************
// Try to read from the ring buffer
//***********************************************************************************
static int RingBufferRead(struct BufferStruct *p)
{
    if (rb.start == NULL)
        return RINGBUFFER_ERR_NULL;

    if (rb.readptr == rb.writeptr)
        return RINGBUFFER_ERR_EMPTY;

    memcpy(p, rb.readptr, sizeof(struct BufferStruct));
    if ((rb.readptr+1) >= rb.end) 
        rb.readptr = rb.start;
    else
        rb.readptr += 1;

    return RINGBUFFER_OK;
}

//***********************************************************************************
// Wait for memory caches etc.
//***********************************************************************************
static void uwait_barrier_sync(void)
{
	int k;
	for (k = 0; k < 10; k++)
		asm volatile("" : : : "memory");
	__sync_synchronize();
}

//***********************************************************************************
// Clock data to fpga and read data back simutaneously
//***********************************************************************************
static uint32_t SpiSendReceive(uint32_t data, int nbits)
{
	uint32_t rdata = 0;
	int i;

	for (i = nbits-1; i >= 0; i--)
	{
		uwait_barrier_sync();
        if (data & (1 << i))
            *gpio_set_high = CFG_SO_BIT;
        else
            *gpio_set_low = CFG_SO_BIT;

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
// Enable the SPI flash chip
//***********************************************************************************
static void FpgaConfigPowerUp()
{
    *gpio_set_low = CFG_SS_BIT;
	SpiSendReceive(0xAB, 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Enable writing to flash
//***********************************************************************************
static void FlashWriteEnable()
{
    *gpio_set_low = CFG_SS_BIT;
	SpiSendReceive(0x06, 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Write n bytes of data to flash at address
//***********************************************************************************
static void FlashWrite(int addr, char *data, int n)
{
    *gpio_set_low = CFG_SS_BIT;
	SpiSendReceive(0x02, 8);
	SpiSendReceive(addr, 24);
	while (n--)
		SpiSendReceive(*(data++), 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Read n bytes of data from flash at address
//***********************************************************************************
static void FlashRead(int addr, char *data, int n)
{
    *gpio_set_low = CFG_SS_BIT;
	SpiSendReceive(0x03, 8);
	SpiSendReceive(addr, 24);
	while (n--)
		*(data++) = SpiSendReceive(0, 8);
    *gpio_set_high = CFG_SS_BIT;
}

//***********************************************************************************
// Wait on flash status 
//***********************************************************************************
static void FlashWait()
{
	while (1)
	{
        *gpio_set_low = CFG_SS_BIT;
		SpiSendReceive(0x05, 8);
		int status = SpiSendReceive(0, 8);
        *gpio_set_high = CFG_SS_BIT;

		if ((status & 0x01) == 0)
			break;

		mdelay(1);
	}
}

//***********************************************************************************
// Write a sector of flash
//***********************************************************************************
static void FlashSector(int addr, char *data, int size)
{
	int i;

	pr_info("Sector 0x%06x .. 0x%06x:", addr, addr+size-1);

    // Erase a 64k block in flash
	FlashWriteEnable();
    *gpio_set_low = CFG_SS_BIT;
	SpiSendReceive(0xd8, 8);
	SpiSendReceive(addr, 24);
    *gpio_set_high = CFG_SS_BIT;
	FlashWait();

	for (i = 0; i < size; i += 256)
	{
		FlashWriteEnable();
		FlashWrite(addr+i, data+i, size-i < 256 ? size-i : 256);
		FlashWait();
	}

    // Read back memory to verify
	for (i = 0; i < size; i += 256)
	{
		char buffer[256];
		FlashRead(addr+i, buffer, size-i < 256 ? size-i : 256);

		if (memcmp(buffer, data+i, size-i < 256 ? size-i : 256)) 
        {
			pr_err("Readback failed\n");
            return;
		}
	}
}

//***********************************************************************************
// Write a bitstream file into the Flash and program the FPGA
//***********************************************************************************
static int FpgaConfigProgram(char *filename)
{
	pr_info("Setting CFG_RST low.\n");
    *gpio_set_low = CFG_RST_BIT;

    int addr = 0;
    int size = 0;
    struct file *file = NULL;

    file = filp_open(filename, O_RDONLY, 0644);
    if (IS_ERR(file))
    {
        pr_err("Failed to open %s, error %ld.\n", filename, PTR_ERR(file));
        return -2;
    }

	if (!(*gpio_read & CFG_DONE_BIT))
    {
		pr_info("CFG_DONE is now low.\n");
	}

    do 
    {
        addr += size;
        size = 0;

        while (size < (int)sizeof(fpgaConfigBuffer)) 
        {
            int rc = kernel_read(file, fpgaConfigBuffer+size, sizeof(fpgaConfigBuffer)-size, &file->f_pos);
            if (rc <= 0) break;
            size += rc;
        }

        if (size > 0)
            FlashSector(addr, fpgaConfigBuffer, size);
    } while (size == sizeof(fpgaConfigBuffer));
    filp_close(file, NULL);

    *gpio_set_low = CFG_RST_BIT;
	mdelay(2);

	pr_info("CFG_RST high.\n");
    *gpio_set_high = CFG_RST_BIT;
	mdelay(500);

	if (!(*gpio_read & CFG_DONE_BIT))
    {
		pr_err("Config programming FPGA failed.\n");
		return -1;
	}

	pr_info("Config success.\n");
	return 0;
}

//***********************************************************************************
// Set pin to be input or output.
// This sets the relevant register to a 3-bit pattern defining the pin as input or
// output. '000' is input, '001' is output
//***********************************************************************************
static void GpioSetPinInOut(int pin, int mode)
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
static void FpgaConfigInitialize(void)
{
	GpioSetPinInOut(CFG_SS,   OUTPUT);
	GpioSetPinInOut(CFG_SCK,  OUTPUT);
	GpioSetPinInOut(CFG_SI,   INPUT);
	GpioSetPinInOut(CFG_SO,   OUTPUT);
    *gpio_set_high = CFG_RST_BIT; // Prevent glitches
	GpioSetPinInOut(CFG_RST,  OUTPUT);
	GpioSetPinInOut(CFG_DONE, INPUT);

    *gpio_set_high = CFG_SS_BIT;
    *gpio_set_low = CFG_SCK_BIT;
    *gpio_set_low = CFG_SO_BIT;
    *gpio_set_high = CFG_RST_BIT;
}

//***********************************************************************************
// Platform driver stuff
//***********************************************************************************
static struct of_device_id fast_gpio_bus_driver_ids[] = {
	{
		.compatible = "fast_gpio_bus,fast_gpio_bus",
	}, {  } // Keep this
};
MODULE_DEVICE_TABLE(of, fast_gpio_bus_driver_ids);

static struct platform_driver fast_gpio_bus_driver = {
	.probe = Probe,
	.remove = Remove,
	.driver = {
		.name = "fast_gpio_bus_driver",
		.of_match_table = fast_gpio_bus_driver_ids,
	},
};

//***********************************************************************************
// Define properties/pins that are in the overlay
//***********************************************************************************
static const char *props[] = {"desc", "version",
    "d0-gpio", "d1-gpio", "d2-gpio", "d3-gpio",
    "d4-gpio", "d5-gpio", "d6-gpio", "d7-gpio", 
    "d8-gpio", "d9-gpio", "d10-gpio", "d11-gpio", 
    "d12-gpio", "d13-gpio", "d14-gpio", "d15-gpio", 
	"cfg-si-gpio", "frame_done-gpio", "unused-18-gpio", "cgf-sck-gpio", 
	"spi1-mosi-gpio", "cfg-so-gpio", "r-en-gpio", "frame-gpio", 
	"cfg-rst-gpio", "cfg-ss-gpio", "cfg-done-gpio", "ext-reset-n-gpio"};
#define NUM_PROPS (sizeof(props)/sizeof(props[0]))
    
static const char *pins[] = {
    "d0", "d1", "d2", "d3",
    "d4", "d5", "d6", "d7", 
    "d8", "d9", "d10", "d11",
    "d12", "d13", "d14", "d15",
	"cfg-si", "frame_done", "unused-18", "cgf-sck", 
	"spi1-mosi", "cfg-so", "r-en", "frame", 
	"cfg-rst", "cfg-ss", "cfg-done", "ext-reset-n"}; 
#define NUM_PINS ((int)(sizeof(pins)/sizeof(pins[0])))

static struct gpio_desc *gpioPins[NUM_PINS] = {
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL, 
    NULL, NULL, NULL, NULL};

static struct proc_dir_entry *proc_file;

//***********************************************************************************
// Called when a user writes to this driver
//***********************************************************************************
static ssize_t Write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) 
{
    pr_info("proc file=%s\n", File->f_path.dentry->d_iname);
	return count;
}

//***********************************************************************************
// ioctl 
// commands:
//  START, STOP, RESET, CONFIG_FPGA
// get requests:
//  GET_REGS, GET_STATS, GET_STATUS, GET_ERROR_ARRAY 
//***********************************************************************************
static long Ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct data_transfer user_data;
    struct Uint32Array10 local_data;
    struct Uint32Array3x25 local_data3x25;

    // Check the magic number for basic error checking
    if (_IOC_TYPE(cmd) != CHR_MAGIC)
        return -ENOTTY; // Inappropriate ioctl for device

    switch (cmd) 
    {
        // START continual read
        case IOCTL_START:
            pr_info("IOCTL START\n");
            if (threadStartTimer == NULL)
            {
                threadStartTimer = kthread_create(kthreadFunction, NULL, "fast_gpio_bus_thread");
                if (IS_ERR(threadStartTimer))
                {
                    pr_err("Failed to create kthread");
                    threadStartTimer = NULL;
                    return PTR_ERR(threadStartTimer);
                }
                kthread_bind(threadStartTimer, 3); // Set affinity for kthread to cpu 3
                wake_up_process(threadStartTimer);
            }
            else
            {
                pr_info("kthread already running - did nothing.\n");
            }
            break;

        // STOP continual read
        case IOCTL_STOP:
            pr_info("IOCTL STOP\n");
            if ((threadStartTimer) && (!(IS_ERR(threadStartTimer))))
            {
                int ret = kthread_stop(threadStartTimer);
                if (ret == -EINTR)
                    pr_err("kthread never started.\n");
                threadStartTimer = NULL;
            }
            else
            {
                pr_info("kthread not started - did nothing.\n");
            }
            break;

        // RESET fpga SSMs 
        case IOCTL_RESET:
            pr_info("IOCTL RESET\n");
            ResetCommunications();
            recCount = 0;
            misses = 0;
            break;

        // CONFIGURE FPGA
        case IOCTL_CONFIG_FPGA:
            pr_info("IOCTL CONFIG FPGA\n");

            if (copy_from_user(&user_data, (struct data_transfer __user *)arg, sizeof(user_data)) != 0) 
            {
                return -EFAULT;
            }

            if (user_data.size == 0 || user_data.size > MAX_STRING_SIZE) 
            {
                return -EINVAL; // Invalid argument
            }
            
            user_data.buffer[MAX_STRING_SIZE - 1] = '\0'; 
            // The string is now safely in kernel memory (user_data.buffer)

            FpgaConfigPowerUp();
            if (FpgaConfigProgram(user_data.buffer) == -2)
                //if (FpgaConfigProgram("/home/d/My_Pi_Projects/RPi-FPGA-01/RPi-FPGA-01.bin") == -2)
            {
                // Could not open file
                return -EFAULT;
            }

            if (!(*gpio_read & CFG_DONE_BIT))
            {
                pr_err("CFG_DONE is 0 - FPGA not programmed.\n");
                return -EFAULT;
            }
            else
            {
                pr_info("CFG_DONE is 1 - FPGA programmed successfully.\n");
            }
            break;

        // Get gpio registers
        case IOCTL_GET_REGS:
            pr_info("IOCTL GET REGS\n");

            local_data.a[0] = *gpio;
            local_data.a[1] = *(gpio + 1);
            local_data.a[2] = *(gpio + 2);
            local_data.a[3] = *gpio_set_high;
            local_data.a[4] = *gpio_set_low;
            local_data.a[5] = *gpio_read;
            local_data.count = 6;

            if (copy_to_user((struct Uint32Array10 *)arg, &local_data, sizeof(local_data)))
            {
                return -EFAULT;
            }
            break;

        // Get statistics
        case IOCTL_GET_STATS:
            pr_info("IOCTL GET STATS\n");

            local_data.a[0] = totalGood;
            local_data.a[1] = misses;
            local_data.a[2] = errors;
            local_data.count = 3;

            if (copy_to_user((struct Uint32Array10 *)arg, &local_data, sizeof(local_data)))
            {
                return -EFAULT;
            }
            break;

        // Get error array
        case IOCTL_GET_ERROR_ARRAY:
            pr_info("IOCTL GET ERROR ARRAY\n");

            int n = recCount;
            if (recCount > 25)
               n = 25;
            for (int i=0; i<n; i++)
            { 
                local_data3x25.a[i] = rec1[i];
                local_data3x25.b[i] = rec2[i];
                local_data3x25.c[i] = rec3[i];
            }
            local_data3x25.count = n;

            if (copy_to_user((struct Uint32Array3x25 *)arg, &local_data3x25, sizeof(local_data3x25)))
            {
                return -EFAULT;
            }
            break;

        // Get status
        case IOCTL_GET_STATUS:
            pr_info("IOCTL GET STATUS\n");

            if (threadStartTimer == NULL)
                local_data.a[0] = 0;
            else
                local_data.a[0] = 1;
            local_data.count = 1;

            if (copy_to_user((struct Uint32Array10 *)arg, &local_data, sizeof(local_data)))
            {
                return -EFAULT;
            }
            break;

        default:
            return -EINVAL; // Invalid argument/command
    }
    return 0; // Success
}

//***********************************************************************************
// Called when a user is reading packets from the /proc file
//***********************************************************************************
static ssize_t Read(struct file *filp, char __user *user_buf, size_t len, loff_t *off)
{
    //pr_info("Started\n");
    struct BufferStruct b;
    int err = RingBufferRead(&b);
    if (err == RINGBUFFER_ERR_EMPTY)
    {
        return 0;
    }
	int not_copied = copy_to_user(user_buf, &b, sizeof(struct BufferStruct));
	if (not_copied) 
	{
    //    pr_err("Did not copy all bytes.");
	}
    pr_info("Done\n");
	return sizeof(struct BufferStruct);
}

//***********************************************************************************
// The timer function - called every 20us
// Read buffers from the FIFO as long as there is data to read (if for some reason
// we stay in here for 1000 buffers, we return our time).
//***********************************************************************************
static enum hrtimer_restart TimerFunction(struct hrtimer *timer) 
{
    int count = 0;
    int bytesRead;
    unsigned int curHead;
    int wrap;

    *gpio_set_high = CFG_SO_BIT;

    while (1)
    {
        bytesRead = ReadBuffer();
        if (bytesRead == 0)
           break; 
        
        readCount++;
        curHead = buf[2] * 256 + buf[1];

        // Check for wrap-around every 1.3 seconds
        wrap = 0;
        if ((curHead == 0) && (lastHead == 65535))
            wrap = 1;

        if (bytesRead < 0)
        {
            // If an error occurred like getting a footer prematurely
            // Count the errors
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
            // The current packet number is not the expected packet number
            // Count the missed packets
            if (recCount < 1000)
            {
                rec1[recCount] = readCount;
                rec2[recCount] = lastHead;
                rec3[recCount] = curHead;
                recCount++;
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
            // No errors - good packet
            totalGood++;
        }
        if (bytesRead == 64)
        {
            // Good packet, write it to the ring buffer
            int err = RingBufferWrite((struct BufferStruct *)buf);
            if (err == RINGBUFFER_ERR_FULL)
            {
            }
        }
        count++;
        if (count > 1000)
            break;

        lastHead = curHead;        
    }

    if (count >= 1000)
    {
        pr_info("1000 bytesRead=%d\n", bytesRead);
    }

    *gpio_set_low = CFG_SO_BIT;

    // Reset the timer to fire again after 20us (since reading a buffer takes 10us
    // maybe this should be 10us when a buffer was read)
    hrtimer_forward_now(timer, ns_to_ktime(HRTIMER_TIME_NS));
	return HRTIMER_RESTART;
}

//***********************************************************************************
// Pulse reset line to reset all the state machines in the FPGA
//***********************************************************************************
void ResetCommunications(void)
{
    *gpio_set_low = EXT_RESET_N_BIT; 
    mdelay(1);
    *gpio_set_high = EXT_RESET_N_BIT; 
}

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

//***********************************************************************************
// This kthread is called from the Probe function when the module is loaded.
// It just starts the high-res timer The only purpose for doing this here is if
// we want to set the affinity for the timer to a particular cpu.
// Note that it uses the HRTIMER_MODE_REL_HARD for *hard* timing (high priority,
// hard irq).
//***********************************************************************************
static int kthreadFunction(void *data) 
{
	// Init of hrtimer
	hrtimer_init(&hrtimerReadData, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
	hrtimerReadData.function = &TimerFunction;

    // Set the R_EN pin low to start
    *gpio_set_low = R_EN_BIT;
    R_EN_state = 0;

    // Reset the fpga state machines
    ResetCommunications();

	hrtimer_start(&hrtimerReadData, ns_to_ktime(HRTIMER_TIME_NS), HRTIMER_MODE_REL_HARD);

    // Basically sleep forever or until explicitly woken up to stop
    while (!kthread_should_stop()) 
    {
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule();
    }
    //threadRunning = 0;
	hrtimer_cancel(&hrtimerReadData);
    return 0;
}

//***********************************************************************************
// Define file operations in this driver
//***********************************************************************************
static struct proc_ops fops = 
{
	.proc_write = Write,
	.proc_read = Read,
    .proc_ioctl = Ioctl,
};

//***********************************************************************************
// Function probe called when driver is registered 
//***********************************************************************************
static int Probe(struct platform_device *pdev) 
{
	struct device *dev = &pdev->dev;
	const char *desc;
	int version;
    int ret;

	pr_info("Started.\n");

    // Set all resources to NULL so we can successfully release them on error
    for (int i=0; i<NUM_PINS; i++)
        gpioPins[i] = NULL;
    proc_file = NULL;
    threadStartTimer = NULL;

    // Get a base pointer to the IO memory for the gpio
    gpio = ioremap(GPIO_BASE_PHYS, GPIO_SIZE);
    if (!gpio) 
    {
        pr_err("Ioremap failed.\n");
        return -ENOMEM;
    }

    // Get useful pointers to registers
	gpio_set_high = gpio + 7;
	gpio_set_low = gpio + 10;
	gpio_read = gpio + 13;

	// Check for device properties
    for (int i=0; i<NUM_PROPS; i++)
    {
        if (!device_property_present(dev, props[i])) 
        {
            pr_err("Device property '%s' not found.\n", props[i]);
            return -ENODATA;
        }
    }
    pr_info("Checked %ld device properties.\n", NUM_PROPS);

	// Read device properties
	ret = device_property_read_string(dev, "desc", &desc);
	if (ret) 
    {
		pr_err("Could not read 'desc'.\n");
		return -ENODATA;
	}
	pr_info("desc= %s\n", desc);
	ret = device_property_read_u32(dev, "version", &version);
	if (ret) 
    {
		pr_err("Could not read 'version'.\n");
		return -ENODATA;
	}
	pr_info("version= %d\n", version);

	// Init GPIO
    // Do not *really* need to do this as we are just going to handle the bcm2711
    // registers directly, but since gpiod is the present and near future, any other
    // attempts to load modules or drivers that use gpiod will fail (or this will fail
    // if modules/drivers are already loaded). 
    for (int i=0; i<NUM_PINS; i++)
    {
        gpioPins[i] = gpiod_get(dev, pins[i], GPIOD_OUT_LOW);
        int err = IS_ERR(gpioPins[i]);
        if (err) 
        {
            pr_err("Could not setup GPIO '%s'.\n", pins[i]);
            ReleaseResources();
            return -ENODEV;
        }
    }
    pr_info("Acquired %d gpio pins.\n", NUM_PINS);

	// Creating procfs file
	proc_file = proc_create("fast-gpio-bus", 0666, NULL, &fops);
	if (IS_ERR(proc_file)) 
    {
		pr_err("Creating /proc/fast-gpio-bus\n");
        ReleaseResources();
		return PTR_ERR(proc_file);
	}

    // Create a ring buffer to hold 1024 packets (at 64bytes = 64kB)
    InitRingBuffer(1024);

    // Setup the FPGA
    FpgaConfigInitialize();

#define PROGRAM_FPGA_IN_PROBE 0
    if (PROGRAM_FPGA_IN_PROBE)
    {
        // Program the FPGA
        FpgaConfigPowerUp();
        pr_info("Programming FPGA\n");
        FpgaConfigProgram("/home/d/My_Pi_Projects/RPi-FPGA-01/RPi-FPGA-01.bin");

        if (!(*gpio_read & CFG_DONE_BIT))
        {
            pr_err("CFG_DONE is 0 - FPGA not programmed - exiting.\n");
            ReleaseResources();
            return -EIO;
        }
    }

#define FLASH_10_TIMES_IN_PROBE 0    
    if (FLASH_10_TIMES_IN_PROBE)
    {
        // Flash GPIO 21 ten times 
        // Set GPIO 21 as output (Function Select Register 2)
        unsigned int val = readl(gpio + 2); // GPFSEL2
        val &= ~(7 << 3);                  // Clear bits 3-5
        val |= (1 << 3);                   // Set as output
        writel(val, gpio + 2);

        int i;
        for (i = 0; i < 10; i++)
        {
            writel(1 << 21, gpio+7); // GPSET0
            mdelay(250);
            writel(1 << 21, gpio+10); // GPSET0
            mdelay(250);
        }
    }

    // Set pins as IO
	GpioSetPinInOut(R_EN, OUTPUT);

#define START_HRTIMER_IN_PROBE 0
    if (START_HRTIMER_IN_PROBE)
    {
        // Create a kthread to start the hrtimer (only really needed when setting
        // cpu affinity otherwise could just create the timer).
        if (threadStartTimer == NULL)
        {
            threadStartTimer = kthread_create(kthreadFunction, NULL, "fast_gpio_bus_thread");
            if (IS_ERR(threadStartTimer))
            {
                pr_err("Failed to create kthread");
                ReleaseResources();
                return PTR_ERR(threadStartTimer);
            }
            kthread_bind(threadStartTimer, 3); // Set affinity for kthread to cpu 3
            wake_up_process(threadStartTimer);
        }
    }

    pr_info("Done.\n");

    *gpio_set_low = EXT_RESET_N_BIT; 
	GpioSetPinInOut(EXT_RESET_N, OUTPUT);
    *gpio_set_high = EXT_RESET_N_BIT; 

	return 0;
}

//***********************************************************************************
// Called when the driver is unregistered or if there is an error while
// initializing, this releases all resources.
//***********************************************************************************
static void ReleaseResources(void) 
{
	pr_info("Started.\n");
    // Release any acquired gpio pins
    for (int i=0; i<NUM_PINS; i++)
    {
        if (gpioPins[i] != NULL)
        {
            if (!IS_ERR(gpioPins[i]))
		        gpiod_put(gpioPins[i]);
        }
    }

    // If files have been created in /proc
    if ((proc_file) && (!(IS_ERR(proc_file))))
	    proc_remove(proc_file);

    DeleteRingBuffer();

    // If the kthread has been created
    if ((threadStartTimer) && (!(IS_ERR(threadStartTimer))))
    {
        kthread_stop(threadStartTimer);
        threadStartTimer = NULL;
        // If the hrtimer has been created
        hrtimer_cancel(&hrtimerReadData);
    }

}

//***********************************************************************************
// Called when the driver is unregistered
//***********************************************************************************
static void Remove(struct platform_device *pdev) 
{
	pr_info("Started.\n");
    ReleaseResources();
	pr_info("Done.\n");
	return;
}

//***********************************************************************************
// Called when the LKM is loaded
//***********************************************************************************
static int __init Initialize(void) 
{
    pr_info("Started.\n");
	if (platform_driver_register(&fast_gpio_bus_driver)) 
    {
		pr_err("Could not load driver.\n");
		return -1;
	}
	pr_info("If you do not see 'probe done' before 'init done', try loading the overlay.\n");
	pr_info("Done.\n");
	return 0;
}

//***********************************************************************************
// Called when the LKM is unloaded
//***********************************************************************************
static void __exit ExitModule(void) 
{
	pr_info("Started.\n");
	platform_driver_unregister(&fast_gpio_bus_driver);
	pr_info("Done.\n");
}

module_init(Initialize);
module_exit(ExitModule);
