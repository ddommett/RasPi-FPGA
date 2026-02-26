# RasPi_FPGA

<a id="readme-top"></a>
RasPi_FPGA is a master project encapsulating all of my open source and open hardware projects that connect a FPGA to the Raspberry Pi 4 for real-time (relatively high-speed) data transfer via GPIO.

Can a Raspberry Pi, running Linux, gather 100 bytes from real-world sensors every 20us without any loss of data by using a custom parallel interface to a FPGA on the GPIO pins? 

YES! 

Best results occur when booting to a text mode console and running software on a dedicated cpu.

Complete documentation of the rationale, experiments, and projects, is in the [documentation](./docs/readme.md).

## Summary

An open source FPGA HAT for the Raspberry Pi was designed and manufactured. It is called RPi-FPGA-01 and uses a Lattice ice40HX4K FPGA. Using the open source software OSS-CAD-Suite, a program for the FPGA was written that fills a FIFO with a 64 byte data packet every 20us (this simulates a suite of real-world sensors collecting data); the FIFO has a depth of 64 packets. 

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA">
    <img src="images/RPi-FPGA-01.jpg" alt="RPi-FPGA-01" width="300" >
  </a>
</div>

Software was written in C to run on a Raspberry Pi 4 to communicate with the FPGA and read data packets from the FPGA. A number of different software approaches were tested in different scenarios. 

Four types of software approaches were developed: (a) a userspace application, (b) a high-resolution timer based kernel module, (c) an IRQ based kernel module, and (d) a bare metal application.

The user space application was tested with no priority, high priority, and on a dedicated cpu. The high-resolution based kernel module also ran on a dedicated cpu. Tests were conducted under a standard Linux kernel, a real-time kernel, and while running Wayland, X11, and text mode console (no GUI). Sysbench, stress-ng, and GeeXLab were used to create test loads on the cpu, memory, file IO, and gpu while the software was tested.  

Images on a scope show the communications working:

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA">
    <img src="images/05-1.png" alt="Scope image 05-1" width="300" >
  </a>
</div>

Sometimes the scope shows lost data packets (green line activating):

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA">
    <img src="images/05-3.png" alt="Scope image 05-3" width="300" >
  </a>
</div>

Here are the results of the tests (smaller bar is better as it indicates a smaller error rate):

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA">
    <img src="images/All-Error-Rates-Chart.png" alt="Chart of Results" width="600" >
  </a>
</div>

Running Wayland is very bad for the real-time task (blue bars), (this will probably improve in future releases of the OS), and there's not any significant difference between the standard kernel (bars without "RT" in their name) and the real-time linux kernel (bars with "RT" in the name).

The best results occurred with "X-cpu" and X-LKM". These tests were on a standard linux kernel running X11. "X-cpu" was the userspace app running on a dedicated cpu core. "X-LKM" was using a kernel module running on a dedicated cpu core. In these two cases, the error rate is approximately 0.001% (one lost packet per 100,000 packets).

The following is the "X-cpu" test, which gave an error rate of 0.00062% (or 0.0000062):

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA">
    <img src="images/nonRT-X11-NanoVG-UserApp-Isolated.png" alt="User App on isolated cpu and GeeXLab NanoVG Test" width="600" >
  </a>
</div>

Booting into a text mode console (no GUI), and running a userspace application on its own dedicated cpu core had zero errors. 

Therefore, the simplest approach to achieving this real-time task is to use the standard Linux kernel (avoid the complexity of building a real-time kernel), and to build a userspace application (avoid the complexity of kernel module development), and run it on a dedicated cpu core. If a GUI is necessary (such as X11), then expect an error rate of 0.001%. If errors cannot be tolerated then boot into text mode. With this, you can achieve a data transfer rate of 3.2MB/s (6.4MB/s if we activate the 16-bit wide bus in the FPGA) via a custom parallel GPIO interface. While this project demonstrated a data rate of 3.2MB/s with a 64-byte packet size, I'm confident 5MB/s could have been achieved with a 100-byte packet, and 10MB/s with a 16-bit wide bus.

[Next-Documentation](./docs/readme.md).




