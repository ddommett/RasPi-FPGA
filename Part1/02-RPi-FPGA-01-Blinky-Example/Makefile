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
# Compiler flags
# -MMD generates *.d dependency files (useful for $(DEP))
CFLAGS	= -MMD -O2 -D_GNU_SOURCE -Wformat=2 -Wall -Wextra -Winline -I. -pipe -fPIC

# Get all the .c files in the folder
SRC = $(wildcard ./*.c)

# Get a list of all the corresponding object files
OBJ = $(SRC:%.c=%.o)

# Get a list of the corresponding dependency files
DEP = $(OBJ:%.o=%.d)

# Include the dependencies here
-include $(DEP)

# List of libraries to link with
LIBS = $(addprefix -l,rt stdc++)

# Use '@' to hide echoing the actual command
%.o: %.c
	@ echo [Compile] $<
	@ gcc $(CFLAGS) -c -o $@ $<

RPi-FPGA-01: $(OBJ)
	gcc -o $@ $(LIBS) RPi-FPGA-01.o

#-------------------------------------------------------------------------------
# Clean the FPGA code
clean:
	rm -f RPi-FPGA-01.json RPi-FPGA-01.asc RPi-FPGA-01.bin

# Clean the C code app
cleanapp:
	rm -f RPi-FPGA-01 $(OBJ) $(DEP) 

.PHONY: clean cleanapp

