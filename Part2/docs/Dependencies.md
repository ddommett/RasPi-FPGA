# Dependencies for Part 2

[Back to Main Part 2](../readme.md)


Icepi Zero requires libftdi which depends on libusb and libconfuse which requires cmake. Run the following script in a terminal to install these dependencies on the Raspberry Pi 5.

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
cd ..
```

It is also necessary to install the free open source FPGA tools (if not done already). Download latest oss-cad-suite-linux-arm64... from github.com/YosysHQ/yosys/oss-cad-suite-build/releases, then run the following:

```
tar -xvf oss-cad*.tgz
```

Set environment with, and add to .bashrc (REPLACE '/home/d' with the correct path to *your* home folder):

```
export PATH="/home/d/oss-cad-suite/bin:$PATH"
sed -i '$a\export PATH="/home/d/oss-cad-suite/bin:$PATH" ' ~/.bashrc 
```

[Back to Main Part 2](../readme.md)





