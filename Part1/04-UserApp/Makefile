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

# Cannot use '$^' to use all object files because some of the *.o are icezappX
RPi-FPGA-01: $(OBJ)
	gcc -o $@ $(LIBS) RPi-FPGA-01.o 

#-------------------------------------------------------------------------------
clean:
	rm -f RPi-FPGA-01 $(OBJ) $(DEP) 

.PHONY: clean 

