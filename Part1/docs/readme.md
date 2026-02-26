# Documentation
<a id="readme-documentation"></a>

## Projects

1. [RPi-FPGA-01](../01-RPi-FPGA-01) - the RPi-FPGA-01 is my own design. It is a circuit board containing a Lattice ice40 FPGA  and a 40-pin header that connects to a Raspberry Pi via the GPIO pins. Open hardware. KiCad project.

2. [RPi-FPGA-01-Blinky-Example](../02-RPi-FPGA-01-Blinky-Example) - a simple project containing verilog code to blink an LED and a simple C app to configure the FPGA.

3. [RPi-FPGA-01-FIFO](../03-RPi-FPGA-01-FIFO) - a verilog design for the FPGA that generates data and fills a 64-packet deep FIFO. 

4. [User-App](../04-UserApp) - a program in C to communicate with the FPGA and read data packets. 

5. [Device-Tree-Overlay](../05-Device-Tree-Overlay) - a device tree overlay to be used with the kernel module projects. 

6. [LKM-Fast-GPIO-Bus](../06-LKM-Fast-GPIO-Bus) - a kernel module called fast_gpio_bus to be inserted into the Linux kernel and to communicate with the FPGA and read data packets (to be used with the device tree overlay and the 'fgb' application). 

7. [fgb](../07-fgb) - a userspace application (fgb = fast gpio bus) for reading data from a kernel module device driver. 

8. [LKM-IRQ](../08-LKM-IRQ) - a kernel module, which uses interrupts (IRQs), called fast_gpio_bus to be inserted into the Linux kernel and to communicate with the FPGA and read data packets (to be used with the device tree overlay and the 'fgb' application). 

9. [Bare-Metal](../09-Bare-Metal) - a 'bare metal' app using [circle](https://github.com/rsta2/circle) to run on the Raspberry Pi 4 without any OS.

## Documentation

1. [Introduction](./Introduction.md) - a short document explaining my goals.

2. [IceZero](./IceZero.md) - instructions for working with Black Mesa Labs' *IceZero FPGA for RasPi* in 2026.

3. [User-App](./UserApp.md) - a program in C to communicate with the FPGA on a RPi-FPGA-01 board and read data packets. 

4. [Fast-GPIO-Bus](./FastGpioBus.md) - a kernel module project (consisting of a device tree overlay, kernel module, and user application) to communicate with the FPGA and read data packets. 

5. [Wayland vs X11](./Wayland_X11.md) - a comparison using Wayland vs X11. 

6. [X11-User-App](./X11-UserApp.md) - re-testing the user app with X11. 

7. [X11-Fast-GPIO-Bus](./X11-FastGpioBus.md) - re-testing the kernel module project with X11. 

8. [Overall Results](./OverallResults.md) - results of all tests in tables and charts. 

9. [Next Steps](./NextSteps.md) - the next steps to achieve the requirements of the real-time task. 



