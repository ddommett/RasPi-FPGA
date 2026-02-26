#-------------------------------------------------------------------------------
# Create the FPGA bitstream binary file 
fpga: RPi-FPGA-01.bin

RPi-FPGA-01.bin: RPi-FPGA-01.asc
	icetime -d hx8k -c 100 RPi-FPGA-01.asc
	icepack RPi-FPGA-01.asc RPi-FPGA-01.bin

RPi-FPGA-01.asc: RPi-FPGA-01.json RPi-FPGA-01.pcf
	nextpnr-ice40 --hx8k --package tq144:4k --json RPi-FPGA-01.json --pcf RPi-FPGA-01.pcf --asc RPi-FPGA-01.asc

RPi-FPGA-01.json: RPi-FPGA-01.v 
	yosys -p 'read_verilog -sv RPi-FPGA-01.v; synth_ice40 -top top -json RPi-FPGA-01.json; stat'

# END Create the FPGA bitstream binary file 


#-------------------------------------------------------------------------------
# Syntax check the verilog
checkverilog:
	iverilog -g2009 RPi-FPGA-01.v	
# END


#-------------------------------------------------------------------------------
# Clean the FPGA code
clean:
	rm -f RPi-FPGA-01.json RPi-FPGA-01.asc RPi-FPGA-01.bin

.PHONY: all clean 

