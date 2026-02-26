#ifndef _FAST_GPIO_BUS_H
#define _FAST_GPIO_BUS_H

#include <linux/ioctl.h>
#define CHR_MAGIC 'c'
#define IOCTL_START         _IO(CHR_MAGIC, 1) // Start continual read
#define IOCTL_STOP          _IO(CHR_MAGIC, 2) // Stop continual read
#define IOCTL_RESET         _IO(CHR_MAGIC, 3) // Reset pulse to fpga SSMs
#define IOCTL_CONFIG_FPGA   _IOW(CHR_MAGIC, 4, int) // Configure the fpga
#define IOCTL_GET_REGS      _IOR(CHR_MAGIC, 11, int) // Get the GPIO registers
#define IOCTL_GET_STATS     _IOR(CHR_MAGIC, 12, int) // Get the error stats
#define IOCTL_GET_ERROR_ARRAY _IOR(CHR_MAGIC, 13, int) // Get the error data array
#define IOCTL_GET_STATUS    _IOR(CHR_MAGIC, 14, int) // Get the status (running)

#define MAX_STRING_SIZE 255

#define RINGBUFFER_OK (0)
#define RINGBUFFER_ERR_NULL (-1)
#define RINGBUFFER_ERR_EMPTY (-2)
#define RINGBUFFER_ERR_FULL (-3)

struct data_transfer
{
    int size;
    char buffer[MAX_STRING_SIZE];
};

struct Uint32Array10
{
    int count;
    unsigned int a[10];
};

struct Uint32Array3x25
{
    int count;
    unsigned int a[25];
    unsigned int b[25];
    unsigned int c[25];
};

struct BufferStruct
{
    unsigned char a[64];
};

#endif
