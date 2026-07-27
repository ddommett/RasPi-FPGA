// ConfigIcepi - Raspberry Pi 5 and Lattice ECP5 FPGA
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

//********************************************************************************
// /usr/bin/c++ -I/usr/include/libftdi1 -O3  -std=gnu++11 -o ConfigIcepi.o -c ConfigIcepi.cpp
// /usr/bin/c++ -O3 ConfigIcepi.o -o ConfigIcepi -lftdi1
// ./ConfigIcepi bitstream.bit
//
// For FTDI API, refer to:
//      https://www.intra2net.com/en/developer/libftdi/documentation/ftdi_8c.html
//********************************************************************************

//********************************************************************************
// Definitions of the JTAG registers in the Lattice ECP5 FPGA: 
// LFE5U_25F_XXBG256.bsdl
// Downloaded from: 
// https://bsdl.info/download.htm?sid=40cb8f52ff2594e988ca59b002289d77
//********************************************************************************

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <ftdi.h>
#include <sys/types.h>
#include <string>

using namespace std;

//********************************************************************************
// DEFINES
//********************************************************************************
#define BUFFER_SIZE 4096

#define REG_STATUS_DONE				(1 << 8)	// Flash or SRAM Done Flag (ISC_EN=0 -> 1 Successful Flash to SRAM transfer, ISC_EN=1 -> 1 Programmed) 
#define REG_STATUS_ISC_EN			(1 << 9)	// Enable Configuration Interface (1=Enable, 0=Disable) 
#define REG_STATUS_FAIL				(1 << 13)	// Fail Flag (1 = Operation failed) 
#define REG_STATUS_CNF_CHK_MASK		(0x0F << 23)	// Configuration Status Check 

#define FLASH_RDSR_WIP  0x01
#define FLASH_RDSR_WEL  0x02

//********************************************************************************
// AN_232R-01_Bit_Bang_Mode_Available_For_FT232R_and_Ft245R
//	FT232RL_TXD = 0,
//	FT232RL_RXD = 1,
//	FT232RL_RTS = 2,
//	FT232RL_CTS = 3,
//	FT232RL_DTR = 4,
//	FT232RL_DSR = 5,
//	FT232RL_DCD = 6,
//	FT232RL_RI  = 7

//********************************************************************************
// GLOBALS
//********************************************************************************
std::vector<unsigned char> bitData;

// Define the 4 'jtag' pins on the FT231X
unsigned char tckPin = 0x20; // FT232RL_DSR;
unsigned char tmsPin = 0x40; // FT232RL_DCD;
unsigned char tdiPin = 0x80; // FT232RL_RI;
unsigned char tdoPin = 0x08; // FT232RL_CTS;
struct ftdi_context *ftdi;
int bufferNum;
unsigned char bufE[BUFFER_SIZE]; // A large buffer for sending
unsigned char bufF[BUFFER_SIZE]; // A large buffer for receiving

//********************************************************************************
// FUNCTIONS
//********************************************************************************
uint64_t GetUint32FromBufF(int offset);
bool WaitNotBusy(void);
void SendBuffer(string tdi, string tms);
unsigned char ReverseByte(unsigned char src);
void WaitFlashStatus(unsigned char mask, unsigned char cond, uint32_t timeout);
uint64_t GetStatusReg(void);
void OpenBitFile(const string &filename);
void ProgramFlash(int eraseOnly);
void ProgramSRAM(void);
int SetBuffer(unsigned char *buf, string tdi, string tms);
void ConvertBitsToBytes(unsigned char *tdo, unsigned char *buf, int numBits);
int MSB_ByteTo16Bits(unsigned char tx, unsigned char *buf);
void SetTmsOnLastBit(unsigned char *buf, int num);
string ByteTo01(unsigned char b);

//********************************************************************************
// JTAG specification defines the Test Access Port (TAP) State Diagram which
// defines how instructions or data are sent through jtag.
// Search online for 'TAP State Diagram' for more information.
//
// A site with an explanation is:
// https://medium.com/@aliaksandr.kavalchuk/diving-into-jtag-protocol-part-1-overview-fbdc428d3a16
//
//********************************************************************************

//********************************************************************************
// MAIN
//********************************************************************************
int main(int argc, char **argv)
{
    if (argc == 1)
    {
        cout << "main <bitstream file>" << endl;
        return 0;
    }

    cout << "Configure (s)ram, (e)rase flash, program (f)lash:";
    unsigned char key = getchar();
    cout << endl;
    cout << "key=" << key << endl;
    if ((key != 's') && (key != 'e') && (key != 'f'))
    {
        cout << "Invalid key pressed." << endl;
        return 0;
    }

    bufferNum = 0;

    // Create a new FTDI context and set it to use interface 'A'
    // Open our device and set the baud rate to 3M
    // The USB Vendor ID for FTDI is 0x0403
    // The Product ID for FT231X chip is 0x6015
    // Set bit mode and clear fifos in chip
    ftdi = ftdi_new();
    ftdi_set_interface(ftdi, (ftdi_interface)1);
    ftdi_usb_open_desc_index(ftdi, 0x0403, 0x6015, NULL, NULL, 0);
    ftdi_set_baudrate(ftdi, 3000000);
    ftdi_set_bitmode(ftdi, tckPin | tmsPin | tdiPin, BITMODE_SYNCBB);
    ftdi_tcioflush(ftdi);

    // clock 5 times with TMS high sets state machine to RESET, one more clock with TMS low goes to IDLE
    SendBuffer(string("") + "000000",
               string("") + "111110");

    OpenBitFile(argv[1]);

    if (key == 'f')    
        ProgramFlash(0);
    else if (key == 's')    
        ProgramSRAM();
    else if (key == 'e')    
        ProgramFlash(1);

    // Cleanup the ftdi context
    ftdi_set_bitmode(ftdi, 0, BITMODE_RESET);
    ftdi_usb_reset(ftdi);

    if (ftdi != NULL)
    {
        ftdi_tciflush(ftdi);
        ftdi_tcoflush(ftdi);
        ftdi_usb_close(ftdi);
        ftdi_free(ftdi);
    }
}

//********************************************************************************
// Convert a byte into a string of ones and zeros
//********************************************************************************
string ByteTo01(unsigned char b)
{
    char s[10] = "00000000";
    unsigned char mask = 0x80;
    for (int i=0; i<8; i++)
    {
        if (b & mask) 
            s[i] = '1';
        mask >>= 1;
    } 
    return string(s);
}    

//********************************************************************************
// Program the Flash on the board
//********************************************************************************
void ProgramFlash(int eraseOnly)
{
    uint64_t reg;
    unsigned char rx[4];
    const unsigned char *ptr = bitData.data();
    int size = 0;
    unsigned char buf[256+4+1];
    int num;
    int len = bitData.size();
    int start_addr = 0;
    int end_addr = (len + 0xffff) & ~0xffff;
    int step = 0x10000;    // block size (64Kb) 
 
    // PRELOAD/SAMPLE = 0x1C = 00011100 --rev--> 00111000  + 26 bytes of 0xFF
    SendBuffer(string("") + "0000" + "00111000" + "00" + "000" + string(25*8, '1') + "11111111" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + string(25*8, '0') + "00000001"+ "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_FAIL != 0) 
    {
        cout << "Preload..ERROR!" << endl;
        return;
    }

    // ISC Enable = 0xC6 = 11000110 --rev--> 01100011
    SendBuffer(string("") + "0000" + "01100011" + "00" + "000" + "00000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000001" + "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_ISC_EN != REG_STATUS_ISC_EN) 
    {
        cout << "Enable ISC ERROR!" << endl;
        return;
    }

    // ERASE_SRAM = 0x0E = 00001110 --rev--> 01110000  + Data Register = 1 
    SendBuffer(string("") + "0000" + "01110000" + "00" + "000" + "10000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000001" + "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_FAIL != 0) 
    {
        cout << "Erase SRAM ERROR!" << endl;
        return;
    }

    // ISC Disable = 0x26 = 00100110 --rev--> 01100100   In-System Configuration Disable 
    SendBuffer(string("") + "0000" + "01100100" + "00",
               string("") + "1100" + "00000001" + "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_ISC_EN != 0) 
    {
        cout << "Disable ISC ERROR!" << endl;
        return;
    }

    // Enter special mode to pass signals through to spi (Instruction Reg 0x3A, Data Reg 0x68)
    // After this, writing to the data register lets us communicate with the flash chip
    // 0x3A = 00111010 --rev--> 01011100   
    // 0x68 = 01101000 --rev--> 00010110   
    SendBuffer(string("") + "0000" + "01011100" + "00" + "000" + "00010110" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000001" + "10");

    // Flash Reset Enable = 0x66 = 01100110
    // (The jtag interface sees the full waveform (e.g TMS = 100 00000001 10) to write to the data register
    // but the flash chip will just see the data on spi_mosi (e.g. 01100110) and the clock on spi_clk)
    SendBuffer(string("") + "000" + "01100110" + "00",
               string("") + "100" + "00000001" + "10");

    // Flash Reset = 0x99 = 10011001
    SendBuffer(string("") + "000" + "10011001" + "00",
               string("") + "100" + "00000001" + "10");

    // JEDEC ID = 0x9F = 10011111 (+ 4 dummy bytes for clocks)
    SendBuffer(string("") + "000" + "10011111" + "00000000" + "00000000" + "00000000" + "00000000" + "00",
               string("") + "100" + "00000000" + "00000000" + "00000000" + "00000000" + "00000001" + "10");

    // Offset 22 to jump over SHIFT_DR and command, 64 -> 32 bits
    ConvertBitsToBytes(rx, bufF+22, 64);
    // The bytes come in as MSB first from the flash 
    rx[0] = ReverseByte(rx[0]);
    rx[1] = ReverseByte(rx[1]);
    rx[2] = ReverseByte(rx[2]);

    if ((rx[0] == 0xef) && (rx[1] == 0x40) && (rx[2] == 0x18))
        std::cout << "JEDEC ID: 0xef4018 Winbond W25Q128 256 sectors size: 128Mb" << std::endl;
    else
        std::cout << "JEDEC ID error ****************************" << std::endl;

    // Erase the sectors in the Flash
    std::cout << "Erasing...";
    printf("From: %08x, To: %08x", 0, (len + 0xffff) & ~0xffff);
    for (int addr = 0; addr < end_addr; addr += step)
    {
        std::cout << "." << std::flush;

        // Flash Write Enable = 0x06 = 00000110
        SendBuffer(string("") + "000" + "00000110" + "00",
                   string("") + "100" + "00000001" + "10");
        WaitFlashStatus(FLASH_RDSR_WEL, FLASH_RDSR_WEL, 1000);

        // Send Erase block command with address of block
        // Flash Block Erase 64k = 0xD8 = 11011000 
        num = SetBuffer(bufE, "000", "100"); // Put go to SHIFT_DR into buffer
        num += SetBuffer(bufE + num, "11011000", "00000000"); // Add block erase command
        num += SetBuffer(bufE + num, ByteTo01(addr >> 16), "00000000"); // Address
        num += SetBuffer(bufE + num, ByteTo01(addr >> 8), "00000000"); // Address
        num += SetBuffer(bufE + num, ByteTo01(addr), "00000001"); // Address
        num += SetBuffer(bufE + num, "00", "10"); // Put go to IDLE into buffer
        ftdi_write_data(ftdi, bufE, num);
        ftdi_read_data(ftdi, bufF, num);

        WaitFlashStatus(FLASH_RDSR_WIP, 0x00, 100000);
    }
    std::cout << "Done" << std::endl;
    if (eraseOnly)
        return;

    // Write data to the Flash
    // Each buffer consists of:
    //  command (1 byte), address (3 or 4 bytes, data (256 bytes or less)
    std::cout << "Writing...";
    for (int addr = 0; addr < len; addr += size, ptr+=size)
    {
        if (addr + 256 > len)
            size = len - addr;
        else
            size = 256;
        std::cout << "." << std::flush;

        num = 0;

        buf[num++] = 0x02;  // Program the flash
        buf[num++] = (addr >> 16);
        buf[num++] = (addr >>  8);
        buf[num++] = addr;
        memcpy(buf + num, ptr, size);

        // Flash Write Enable = 0x06 = 00000110
        SendBuffer(string("") + "000" + "00000110" + "00",
                   string("") + "100" + "00000001" + "10");


        WaitFlashStatus(FLASH_RDSR_WEL, FLASH_RDSR_WEL, 1000);

        // Actually send the buffer of data
        num += size;
        bufferNum = 0;
        SendBuffer(string("") + "000", // Go to SHIFT_DR
                   string("") + "100");

        ftdi_set_bitmode(ftdi, tckPin | tmsPin | tdiPin, BITMODE_BITBANG);
        ftdi_tcioflush(ftdi);

        for (int i = 0; i < num; i++)
        {
            bufferNum += MSB_ByteTo16Bits(buf[i], bufE + bufferNum);
            if (i == (num - 1))
                SetTmsOnLastBit(bufE, bufferNum);

            if ((bufferNum >= BUFFER_SIZE) || (i == (num-1)))
            {
                ftdi_write_data(ftdi, bufE, bufferNum);
                bufferNum = 0;
            }
        }
        ftdi_set_bitmode(ftdi, tckPin | tmsPin | tdiPin, BITMODE_SYNCBB);
        ftdi_tcioflush(ftdi);

        SendBuffer(string("") + "00", // Go to IDLE
                   string("") + "10");

        WaitFlashStatus(FLASH_RDSR_WIP, 0x00, 1000);
    }

    // ISC Refresh 0x79 = 0x01111001 --rev-> 10011110
    SendBuffer(string("") + "0000" + "10011110" + "00",
               string("") + "1100" + "00000001" + "10");

    WaitNotBusy();
    reg = GetStatusReg();
    if (reg & REG_STATUS_DONE)
        cout << "Success!" << endl;
    else
        cout << "ERROR!" << endl;
}

//********************************************************************************
// Given a waveform representing the tdi and tms signals in strings of ones and
// zeros (equal length) - send this buffer up the USB and read back a same length
// buffer 
//********************************************************************************
void SendBuffer(string tdi, string tms)
{
    int num = SetBuffer(bufE, tdi, tms);
    ftdi_write_data(ftdi, bufE, num);
    ftdi_read_data(ftdi, bufF, num);
}

//********************************************************************************
// Get the 64-bit status register and return it 
//********************************************************************************
uint64_t GetStatusReg(void)
{
    uint64_t reg = 0;
    unsigned char rx[8];
    
    // 0x3C = 00111100 --rev--> 00111100 // LSC_READ_STATUS  + 8 bytes of 0x00
    SendBuffer(string("") + "0000" + "00111100" + "00" + "000" + string(8*7, '0') + "00000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + string(8*7, '0') + "00000001" + "10");
    
    ConvertBitsToBytes(rx, bufF+34, 16 * 8);
    
    for (int i=7; i>=0; i--)
        reg = (reg <<= 8) + rx[i];

    return reg;
}

//********************************************************************************
// Starting at an offset in bufF, read a 32-bit unsigned int
//********************************************************************************
uint64_t GetUint32FromBufF(int offset)
{
    uint32_t idCode2B;
    unsigned char devIDB[4];
    ConvertBitsToBytes(devIDB, bufF+offset, 64);
    idCode2B = devIDB[3] << 24 | devIDB[2] << 16 | devIDB[1] << 8  | devIDB[0];
    return idCode2B;
}

//********************************************************************************
// Program the FPGA SRAM
//********************************************************************************
void ProgramSRAM(void)
{
    // ID Code = 0xE0 = 11100000 --rev--> 00000111
    SendBuffer(string("") + "0000" + "00000111" + "00" + "000" + "00000000000000000000000000000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000000000000000000000000000001" + "10");

    uint32_t idCode2B = GetUint32FromBufF(34);
    if (idCode2B != 0x41111043)
    {
        cout << "JEDEC ID..";
        printf("0x%08X\n", idCode2B);
        cout << "JEDEC ERROR!" << endl;
        return; 
    }

    // PRELOAD/SAMPLE = 0x1C = 00011100 --rev--> 00111000  + 26 bytes of 0xFF
    SendBuffer(string("") + "0000" + "00111000" + "00" + "000" + string(25*8, '1') + "11111111" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + string(25*8, '0') + "00000001"+ "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_FAIL != 0) 
    {
        cout << "Preload..ERROR!" << endl;
        return;
    }

    // ISC Enable = 0xC6 = 11000110 --rev--> 01100011
    SendBuffer(string("") + "0000" + "01100011" + "00" + "000" + "00000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000001" + "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_ISC_EN != REG_STATUS_ISC_EN) 
    {
        cout << "Enable ISC ERROR!" << endl;
        return;
    }

    // ERASE_SRAM = 0x0E = 00001110 --rev--> 01110000  + Data Register = 1 
    SendBuffer(string("") + "0000" + "01110000" + "00" + "000" + "10000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000001" + "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_FAIL != 0) 
    {
        cout << "Erase SRAM ERROR!" << endl;
        return;
    }
    
    // Send the bitstream!
    // LSC Bitstream Burst = 0x7A = 01111010 --rev--> 01011110 
    SendBuffer(string("") + "0000" + "01011110" + "00",
               string("") + "1100" + "00000001" + "10");

    // Get data in chunks and send it to SRAM
    const unsigned char *data = bitData.data();
    int length = bitData.size();

    std::cout << "Programming SRAM";

    // Move state machine to SHIFT_DR 
    SendBuffer(string("") + "000",
               string("") + "100");

    ftdi_set_bitmode(ftdi, tckPin | tmsPin | tdiPin, BITMODE_BITBANG);
    ftdi_tcioflush(ftdi);

    bufferNum = 0;
    int i;
    for (i = 0; i < (length-1); i++)
    {
        bufferNum += MSB_ByteTo16Bits(data[i], bufE + bufferNum);
        if (bufferNum >= BUFFER_SIZE)
        {
            std::cout << "." << std::flush;
            ftdi_write_data(ftdi, bufE, bufferNum);
            bufferNum = 0;
        }
    }
    bufferNum += MSB_ByteTo16Bits(data[i], bufE + bufferNum);
    SetTmsOnLastBit(bufE, bufferNum);
    std::cout << "." << std::flush;
    ftdi_write_data(ftdi, bufE, bufferNum);

    ftdi_set_bitmode(ftdi, tckPin | tmsPin | tdiPin, BITMODE_SYNCBB);
    ftdi_tcioflush(ftdi);
    // Move state machine to IDLE 
    SendBuffer(string("") + "00",
               string("") + "10");

    // Check configuration of SRAM was successful
    if (GetStatusReg() & REG_STATUS_CNF_CHK_MASK != 0) 
    {
        cout << "Configuration ERROR!" << endl;
        return;
    }

    // ISC Disable = 0x26 = 00100110 --rev--> 01100100   In-System Configuration Disable 
    SendBuffer(string("") + "0000" + "01100100" + "00",
               string("") + "1100" + "00000001" + "10");
    WaitNotBusy();
    if (GetStatusReg() & REG_STATUS_ISC_EN != 0) 
    {
        cout << "Disable ISC ERROR!" << endl;
        return;
    }

    std::cout << "Success!" << endl;
}

//********************************************************************************
// tdi and tms are equal length strings containing ones and zeros
// Convert these two strings into actual bytes for the tdi and tms pins and put 
// into buffer. Every even numbered byte gets tck.
//********************************************************************************
int SetBuffer(unsigned char *buf, string tdi, string tms)
{
    int len = tdi.length();

    for (int i=0; i<len; i++)
    {
        *buf = 0;
        if (tdi[i] == '1')
            *buf |= tdiPin;
        if (tms[i] == '1')
            *buf |= tmsPin;
        *(buf+1) = *buf;
        *(buf+1) |= tckPin;
        buf += 2;
    }
    return len * 2;
}

//********************************************************************************
// Open the bitstream file and read it and parse the header
//********************************************************************************
void OpenBitFile(const string &filename)
{
    int fileSize;
    std::string rawData;
    int currPos = 0;

    // Open the bit file and read into a string
    FILE *f = fopen(filename.c_str(), "rb");
    if (!f)
        throw std::runtime_error("Error: fail to open " + filename);

    fseek(f, 0, SEEK_END);
    fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    rawData.resize(fileSize);

    fread((char *)&rawData[0], sizeof(char), fileSize, f);
    fclose(f);

    bitData.reserve(fileSize);

    // Parse header until 0xFFFFBDB3 0xFFFF

    // Comment area  
    if ((rawData[0] != 0xFF) || (rawData[1] != 0x00)) 
        throw std::runtime_error("Error: No 0xFF00 in bit file");
    currPos+=2;

    // Find preamble
    size_t endHeader = rawData.find(0xFF, currPos);
    size_t pos = rawData.find(0xB3, endHeader);
    endHeader = pos - 4; 

    // check preamble 
    uint32_t preamble = (*(uint32_t *)&rawData[endHeader + 1]);
    printf("Preamble: %08X\n", preamble);

    // Extract idcode from configuration data (area starting with 0xE2)
    unsigned char *ptr;
    pos = endHeader + 5;  // drop 16 Dummy bits and preamble
    bool found = false;
    uint32_t idCode;
    while (!found && (pos < rawData.size())) 
    {
        unsigned char cmd = (unsigned char) rawData[pos++];
        switch (cmd)
        {
            case 0xFF: // bypass
                break;
            case 0x3B: // LSC_RESET_CRC
                pos += 3;
                break;
            case 0xE2: // ID
                ptr = (unsigned char*)&rawData[pos];
                idCode = (((uint32_t)ptr[3]) << 24) | (((uint32_t)ptr[4]) << 16) | (((uint32_t)ptr[5]) <<  8) | ((uint32_t)ptr[6]);
                found = true;
                break;
        }
    }

    // Remove header from raw data
    bitData.resize(rawData.size() - endHeader);
    std::move(rawData.begin() + endHeader, rawData.end(), bitData.begin());

    // Read Device ID
    uint32_t idCode2;
    unsigned char devID[4];
    unsigned char buf[64];
    
    // ID Code = 0xE0 = 11100000 --rev--> 00000111
    SendBuffer(string("") + "0000" + "00000111" + "00" + "000" + "00000000000000000000000000000000" + "00",
               string("") + "1100" + "00000001" + "10" + "100" + "00000000000000000000000000000001" + "10");

    uint32_t idCode2B = GetUint32FromBufF(34);
    // Offset 22 to jump over SHIFT_DR, 64 -> 32 bits
//    ConvertBitsToBytes(devID, bufF+6, 64);
    if (idCode2B != 0x41111043)
    {
        cout << "JEDEC ID..";
        printf("0x%08X\n", idCode2B);
        cout << "JEDEC ERROR!" << endl;
        return; 
    }

    idCode2 = devID[3] << 24 | devID[2] << 16 | devID[1] << 8  | devID[0];
    printf("ID Code: 0x%08X\n", idCode);
    int ver = idCode >> 28;
    int part = (idCode & 0x0FFFF000) >> 12;
    int manu = (idCode & 0x00000FFE) >> 1;
    printf("ID Code: 0x%08X, Manufacturer: 0x%04X, Part: 0x%04X, Ver: 0x%02X\n", idCode, manu, part, ver);
    std::cout << "Open file: bitstream.bit" << std::endl;
}

//********************************************************************************
// Wait until not busy
//********************************************************************************
bool WaitNotBusy(void)
{
    unsigned char rx;
    int timeout = 0;
    
    // Send 10 clocks    
    SendBuffer("0000000000", 
               "0000000000");
    do
    {
        // READ_BUSY_FLAG =	0xF0 = 11110000 --rev--> 00001111 
        SendBuffer(string("") + "0000" + "00001111" + "00" + "000" + "00000000" + "00",
                   string("") + "1100" + "00000001" + "10" + "100" + "00000001" + "10");

        ConvertBitsToBytes(&rx, bufF+34, 16);
        if (timeout++ == 100000000)
        {
            std::cerr << "timeout" << std::endl;
            return false;
        }
    } while (rx != 0);

    return true;
}

//********************************************************************************
// Reverse the bits in a byte (i.e. 0x01010111 -> 0x11101010)
//********************************************************************************
unsigned char ReverseByte(unsigned char src)
{
    unsigned char dst = 0;
    for (int i=0; i < 8; i++) 
    {
        dst = (dst << 1) | (src & 0x01);
        src >>= 1;
    }
    return dst;
}

//******************************************************************************** 
// When writing data, the bytes must be converted to bits. Each byte is turned
// into 16 bytes where each pair of bytes is a single bit with clock low then high
// It will add this to the 'buffer'
//******************************************************************************** 
int MSB_ByteTo16Bits(unsigned char tx, unsigned char *buf)
{
    for (int j=0; j<8; j++)
    {
        *buf++ = tx & tdiPin;
        *buf++ = *(buf-1) | tckPin;
        tx <<= 1;
    }
    return 16;
}

//********************************************************************************
// Set the TMS pin on the last bit in the buffer
//********************************************************************************
void SetTmsOnLastBit(unsigned char *buf, int num)
{
    buf[num-1] |= tmsPin;
    buf[num-2] |= tmsPin;
}

//********************************************************************************
// Wait for flash to be done 
// Continually read a register, mask it and check if it equals the 'cond' (flag
// set or cleared)
// Timeout after 'timeout' reads.
//********************************************************************************
void WaitFlashStatus(unsigned char mask, unsigned char cond, uint32_t timeout)
{
    unsigned char rx;

    // Flash Read Status Register = 0x05 = 00000101
    SendBuffer(string("") + "000" + "00000101",
               string("") + "100" + "00000000");

    do
    {
        // Repeat read
        SendBuffer(string("") + "00000000",
                   string("") + "00000000");
        ConvertBitsToBytes(&rx, bufF, 16); // A byte is in 16 bits

        if ((ReverseByte(rx) & mask) == cond)
            break;
    } while (timeout-- >= 2);

    // clock another byte with TMS high to get to EXIT state, then go to IDLE
    SendBuffer(string("") + "000" + "00000000" + "00",
               string("") + "100" + "00000001" + "10");
}

//******************************************************************************** 
// When reading data, the data is in 'every other' byte in the buffer, starting
// at byte 1 (not zero). So 16 bytes in the read buffer equals a single actual
// byte of data.
// e.g. 0 1       2 3  4 5  ... 14 15
//      X LSB(B0) X B1 X B2 ... X  MSB
// This function will look at numBits * 2 bytes in the variable 'buffer' and
// write bytes to *tdo.      
//******************************************************************************** 
void ConvertBitsToBytes(unsigned char *tdo, unsigned char *buf, int numBits)
{
    int offset = 0;
    for (int i=1; i<numBits; i += 16)
    {
        tdo[offset] = 0;

        if (buf[i] & tdoPin)
            tdo[offset] |= 0x01;
        if (buf[i+2] & tdoPin)
            tdo[offset] |= 0x02;
        if (buf[i+4] & tdoPin)
            tdo[offset] |= 0x04;
        if (buf[i+6] & tdoPin)
            tdo[offset] |= 0x08;
        if (buf[i+8] & tdoPin)
            tdo[offset] |= 0x10;
        if (buf[i+10] & tdoPin)
            tdo[offset] |= 0x20;
        if (buf[i+12] & tdoPin)
            tdo[offset] |= 0x40;
        if (buf[i+14] & tdoPin)
            tdo[offset] |= 0x80;
        offset++;
    }
}
