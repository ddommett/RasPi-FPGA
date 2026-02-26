// fgb - Raspberry Pi 4 and Lattice ice40HX4K FPGA
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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h> // Required for ioctl()

#include "../include/fast_gpio_bus.h"

void readTest(int f, int numSeconds)
{
    struct BufferStruct buf;
    ssize_t bytes_read;
    unsigned int lastHead;
    unsigned int curHead;
    time_t t1;
    time_t t2;
    unsigned int numReads;
    unsigned int recCount;
    unsigned int rec1[1000];
    unsigned int rec2[1000];
    unsigned int rec3[1000];

    //-------------------------------------
    {
        // do 10000 reads to clear ring buffer

        for (int i=0; i<10000; i++)
        {
            bytes_read = read(f, &buf, sizeof(buf));
            if (bytes_read == 0)
                break;
        }

        lastHead = buf.a[2] * 256 + buf.a[1];
        t1 = time(NULL);
        t2 = time(NULL);
        numReads = 0;
        recCount = 0;
        while ((t2-t1) < numSeconds)
        {
            bytes_read = read(f, &buf, sizeof(buf));
            if (bytes_read == sizeof(buf)) 
            {
                numReads++;
                curHead = buf.a[2] * 256 + buf.a[1];
                int wrap = 0;
                if ((curHead == 0) && (lastHead == 65535))
                    wrap = 1;
                if ((!wrap) && (curHead != (lastHead+1)))
                {   
                    if (recCount < 1000)
                    {
                        rec1[recCount] = numReads;
                        rec2[recCount] = lastHead;
                        rec3[recCount] = curHead;
                        recCount += 1;
                    }
                }
                lastHead = curHead;
            } 
            t2 = time(NULL);
        }
	int lost = 0;
	int totLost = 0;
        for (unsigned int i=0; i<recCount; i++)
        {
            printf("%d: %d %d\n", rec1[i], rec2[i], rec3[i]);
	    lost = rec3[i] - rec2[i] - 1;
	    totLost += lost;
        }
        printf("numreads=%d, recCount=%d, total Lost=%d\n", numReads, recCount, totLost);
    }
}
 
int main(int argc, char **argv)
{
    const char *dev = "/proc/fast-gpio-bus";
    struct Uint32Array10 a;
    struct Uint32Array3x25 a3x25;
    const char *fName = "/home/d/My_Pi_Projects/RPi-FPGA-01/RPi-FPGA-01.bin";
    struct data_transfer fNameStruct;
    struct BufferStruct buf;

	if (argc < 2) {
		printf("Usage: %s GET_STATS     ... get stats\n", argv[0]);
		printf("Usage: %s GET_REGS      ... get GPIO registers\n", argv[0]);
		printf("Usage: %s GET_STATUS    ... get status (running)\n", argv[0]);
		printf("Usage: %s GET_ERROR_ARRAY ... get 25 error array\n", argv[0]);
		printf("Usage: %s START         ... START continual reads\n", argv[0]);
		printf("Usage: %s STOP          ... STOP continual reads\n", argv[0]);
		printf("Usage: %s RESET         ... RESET fpga SSMs\n", argv[0]);
		printf("Usage: %s CONFIG_FPGA   ... configure the fpga with a .bin file\n", argv[0]);
		printf("Usage: %s READ1         ... READ 1 buffer\n", argv[0]);
		printf("Usage: %s READ10SEC     ... READ buffers for 10s\n", argv[0]);
		printf("Usage: %s READ1MIN      ... READ buffers for 1min\n", argv[0]);
		printf("Usage: %s READ1HOUR     ... READ buffers for 1hour\n", argv[0]);
		return 1;
	}

    int f = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);

    if (f < 0) 
    {
        printf("Error opening device: %s, errno=%d (%s)\n", "/dev/ttyUSB0", errno, strerror(errno));
        return 1;
    }

    //-------------------------------------
	if (strcmp(argv[1], "GET_STATS") == 0)
    {
        if (ioctl(f, IOCTL_GET_STATS, &a) != 0) 
        {
            printf("Error %i from ioctl IOCTL_GET_STATS: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- Statistics ----------\n");
            printf("good=%d, misses=%d, errors=%d\n", a.a[0], a.a[1], a.a[2]);
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "GET_REGS") == 0)
    {
        if (ioctl(f, IOCTL_GET_REGS, &a) != 0) 
        {
            printf("Error %i from ioctl IOCTL_GET_REGS: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- GPIO Registers ----------\n");
            printf("GPIO_FSB0 0x%08X\n", a.a[0]);
            printf("GPIO_FSB1 0x%08X\n", a.a[1]);
            printf("GPIO_FSB2 0x%08X\n", a.a[2]);
            printf("GPIO_SET  0x%08X\n", a.a[3]);
            printf("GPIO_CLR  0x%08X\n", a.a[4]);
            printf("GPIO_READ 0x%08X\n", a.a[5]);
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "GET_STATUS") == 0)
    {
        if (ioctl(f, IOCTL_GET_STATUS, &a) != 0) 
        {
            printf("Error %i from ioctl IOCTL_GET_STATUS: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- Status ----------\n");
            if (a.a[0] == 1)
                printf("running\n");
            else
                printf("not running\n");
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "GET_ERROR_ARRAY") == 0)
    {
        if (ioctl(f, IOCTL_GET_ERROR_ARRAY, &a3x25) != 0) 
        {
            printf("Error %i from ioctl IOCTL_GET_ERROR_ARRAY: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- Errors ----------\n");
            for (int i=0; i<a3x25.count; i++)
            {
                printf("%d: %d %d\n", a3x25.a[i], a3x25.b[i], a3x25.c[i]);
            }
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "START") == 0)
    {
        if (ioctl(f, IOCTL_START, NULL) != 0) 
        {
            printf("Error %i from ioctl IOCTL_START: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- START ----------\n");
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "STOP") == 0)
    {
        if (ioctl(f, IOCTL_STOP, NULL) != 0) 
        {
            printf("Error %i from ioctl IOCTL_STOP: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- STOP ----------\n");
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "RESET") == 0)
    {
        if (ioctl(f, IOCTL_RESET, NULL) != 0) 
        {
            printf("Error %i from ioctl IOCTL_RESET: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- RESET ----------\n");
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "CONFIG_FPGA") == 0)
    {
        strcpy(fNameStruct.buffer, fName);
        fNameStruct.size = strlen(fName);
        if (ioctl(f, IOCTL_CONFIG_FPGA, &fNameStruct) != 0) 
        {
            printf("Error %i from ioctl IOCTL_CONFIG_FPGA: %s\n", errno, strerror(errno));
        } 
        else 
        {
            printf("---------- Configure FPGA ----------\n");
        }
    }
 
    //-------------------------------------
	if (strcmp(argv[1], "READ1") == 0)
    {
        ssize_t bytes_read = read(f, &buf, sizeof(buf));

        if (bytes_read == sizeof(buf)) 
        {
            for (int i=0; i<8; i++)
            {
                for (int j=0; j<8; j++)
                {
                    printf("0x%02X ", buf.a[i*8 + j]);
                }
                printf("\n");
            }
        } 
        else 
        {
            printf("READ1 ERROR bytes_read=%ld", bytes_read);
        }
    }
 
    //-------------------------------------
    if (strcmp(argv[1], "READ10SEC") == 0)
    {
	    readTest(f, 10);
	    printf("After 10 seconds\n");
    }
 
    //-------------------------------------
    if (strcmp(argv[1], "READ1MIN") == 0)
    {
	    readTest(f, 60);
	    printf("After 1 minute\n");
    }
 
    //-------------------------------------
    if (strcmp(argv[1], "READ1HOUR") == 0)
    {
	    readTest(f, 60*60);
	    printf("After 1 hour\n");
    }
 
	return 0;
}

