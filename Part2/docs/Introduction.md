# RasPi-FPGA Part 2

[Back to Main Part 2](../readme.md)

<a id="readme-top"></a>
It was now time to step up to a more powerful FPGA **and** the newer, more powerful, Raspberry Pi 5. 

The Icepi Zero, available on [Crowdsupply - Icepi Zero](https://www.crowdsupply.com/icy-electronics/icepi-zero), and further documented at [Github - IcepiZero](https://github.com/cheyao/icepi-zero) is a nice open-source board that contains the next logical FPGA to work with from Lattice - an ECP5. This FPGA is bigger (physically smaller BGA package, but larger internal logic) and faster than the ice40HX4K and also contains extra features like LVDS capable IO pins. It has a 40-pin GPIO header to connect with a Raspberry Pi and a GDPI (video) connector. This board should be useful to experiment with the idea of using a HDMI-to-CSI adapter and video capture to transfer data from the outside world into a Raspberry Pi at high speed.

I purchased the IcePi Zero as an early backer of the project on Crowdsupply and I happily connected it to my Raspberry Pi 4 that I had used for my previous work with the RasPi-FPGA. Using [openFPGALoader](https://github.com/trabucayre/openFPGALoader), I programmed it (via USB) with an example bitstream and watched the LEDs blink - success! I then connected it to a Raspberry Pi 5 and hit my first challenge, as it failed to program with openFPGALoader. *Update - a couple of months later, OpenFPGALoader works! Maybe I encountered a glitch or maybe a patch or update fixed something.*

So I thought I would take a step back and write my own software to program the ECP5 on the Icepi Zero via the USB and the FTDI FT231X IC that is on the board, and this entailed learning more about the FT231X and the JTAG interface. 

Dependencies for Part 2: [Dependencies.md](./Dependencies.md).

Documentation of the JTAG: [JTAG.md](./JTAG.md).

Before I attempt to use the Icepi Zero to output a suitable HDMI signal for capture, I thought it would be beneficial to understand the process of capturing video from a C790 HDMI-to-CSI2 bridge adapter board attached to a Raspberry Pi 5. See this link: [HDMI2CSI-Start](./HDMI2CSI-Start.md)

Go to [Icepi-Zero-HDMI-640x480.md](./Icepi-Zero-HDMI-640x480.md) for the demonstration of sending encoded data via HDMI at 640x480 through a C790 HDMI to CSI2 adapter into a Raspberry Pi 5 at 55MBps.

[Back to Main Part 2](../readme.md)
