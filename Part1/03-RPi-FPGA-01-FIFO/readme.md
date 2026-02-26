# RPi-FPGA-01-FIFO

<a id="readme-rpi-fpga-01-fifo"></a>
Requirements: oss-cad-suite from [https://github.com/YosysHQ/oss-cad-suite-build/releases)](https://github.com/YosysHQ/oss-cad-suite-build/releases). 

Download the latest oss-cad-suite from [https://github.com/YosysHQ/oss-cad-suite-build/releases)](https://github.com/YosysHQ/oss-cad-suite-build/releases). For me, on a Raspberry Pi 400, I downloaded the latest 64-bit arm version: oss-cad-suite-linux-arm64-*.tgz and I copied it to my home folder. Then I unzipped it and added it to my path variable (also added it to my ~/.bashrc with a sed command so it would remain active after rebooting):

```bash
tar -xvf oss*.tgz
export PATH="/home/d/oss-cad-suite/bin:$PATH"
sed -i '$a\export PATH="/home/d/oss-cad-suite/bin:$PATH" ' ~/.bashrc 
```
(/home/d is *my* home folder - change it to match your home folder.) You can now verify oss-cad-suite is installed by typing:

```bash
yosys
```

and you should see yosys start (press CTRL-z to exit).

To make the .bin file for the FPGA:

```bash
make
```

See the project "User App" for how to program the FPGA with this .bin file.

## License
The RPi-FPGA-01-FIFO is an open source project licensed under GPLv3.  Please see the included LICENSE file for details.  If you do wish to distribute software derived from this open source software project then you must also release the source files for the software under GPLv3.  You are free to do this, but please improve upon the original design and provide a tangible benefit for users of the software.

