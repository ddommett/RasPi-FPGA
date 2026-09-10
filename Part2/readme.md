# RasPi-FPGA Part 2

Can a Raspberry Pi 5, running Linux, gather 100 bytes from real-world sensors every 20us without any loss of data by using a the CSI2 input?

YES! 

Using an encoded HDMI resolution of 640x480 @ 60fps, 55MBps is achieved!

Complete documentation of the rationale, experiments, and projects, is in the [documentation](./docs/Introduction.md).

## TL;DR Summary (Valid July 2026)

Use the [Icepi Zero](https://github.com/cheyao/icepi-zero), with a Lattice ECP5 FPGA, and [Geekworm C790 HDMI-to-CSI2](https://geekworm.com/products/c790?) to feed data from a FPGA at a high data rate into a Raspberry Pi 5.

### Obtain Icepi Zero and C790, Connect them to a Raspberry Pi 5

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA2">
    <img src="./images/HDMI-CSI2-Hardware-Setup.jpg" alt="The hardware setup." width="543" >
  </a>
</div>

### Install Dependencies

```
wget https://github.com/libconfuse/libconfuse/releases/download/v3.3/confuse-3.3.tar.gz
tar xf confuse-3.3.tar.gz
cd confuse-3.3
./configure && make -j9
sudo make install
sudo ldconfig
cd ..

sudo apt install -y libusb-1.0-0-dev cmake

wget https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.5.tar.bz2
tar -xvf libftdi*.bz2
cd libftdi1-1.5
mkdir build
cd build
cmake -DMAKE_INSTALL_PREFIX="/usr" ../
make
sudo make install
cd ../..
```

Download latest oss-cad-suite-linux-arm64... from github.com/YosysHQ/oss-cad-suite-build/releases

Unpack Oss-Cad-Suite:

```
tar -xvf oss-cad*.tgz
```

Set environment with, and add to .bashrc (REPLACE '/home/d' with the correct path to *your* home folder):

```
export PATH="/home/d/oss-cad-suite/bin:$PATH"
sed -i '$a\export PATH="/home/d/oss-cad-suite/bin:$PATH" ' ~/.bashrc 
```

### Specify Linux OS Overlays

```
sudo sed -i '$a\dtoverlay=tc358743,4lane=1 ' /boot/firmware/config.txt 
sudo sed -i '$a\dtoverlay=tc358743-audio ' /boot/firmware/config.txt 
```

Reboot!

### Set up HDMI-CSI2 Scripts

Copy 80-video.rules to /etc/udev/rules.d, load the new udev rules, and make the C790 script executable:

```
cd HDMI-CSI2
sudo cp 80-video.rules /etc/udev/rules.d
sudo udevadm control --reload-rules
sudo udevadm trigger
chmod 777 C790-640x480p
cd ..
```

### HDMI Data from FPGA

Make sure the Icepi Zero is connected because it is about to be configured.

```
cd 21-Icepi-Zero-640x480
make build
make
cd ..
```

The following picture shows an example of this firmware running and the output being captured via ffplay:

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA2">
    <img src="./images/HDMI_CSI2_Example1.png" alt="Example of encoded data in 640x480 HDMI signal." width="543" >
  </a>
</div>

### Capturing HDMI frames in a C App.

Run a C app to capture frames, "decode" the data, and check for errors. Install dependencies, compile and run the capture app. (Run the C790 script first now that the Icepi Zero is sending video into the C790.)

```
cd HDMI-CSI2
./C790-640x480p
cd ..
sudo apt install -y build-essential cmake libopencv-dev
cd 22-Capture-HDMI-640x480
g++ Capture-640x480.cpp -o Capture-640x480 `pkg-config --cflags --libs opencv4`
./Capture-640x480
```

The following picture shows the software running (not very exciting):

<div align="center">
  <a href="https://github.com/ddommett/RasPi_FPGA2">
    <img src="./images/HDMI_CSI2_Example2.png" alt="Example of encoded data in 640x480 HDMI signal." width="543" >
  </a>
</div>

Note that the C app is running in userspace (not kernel space), has no special priorities associated with the process, is running under Wayland on a windowed GUI, AND is calling a "printf" like function to print to the terminal. It doesn't to write directly to any cpu registers, or do any "bit-banging". A resolution of 640x480 with 24 bits per pixel is a data rate of approximately 55MBps!

