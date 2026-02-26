obj-m += fast_gpio_bus.o 

all: fgb
	echo Built fgb

clean:
	rm -f fgb fgb.o 
	
#-------------------------------------------------------------------------------
# Compiler flags
# -MMD generates *.d dependency files (useful for $(DEP))
CFLAGS	= -O2 -D_GNU_SOURCE -Wformat=2 -Wall -Wextra -Winline -I. -pipe -fPIC

# Get all the .c files in the folder
#SRC = $(wildcard ./*.c)
#SRC = fgb.c

# Get a list of all the corresponding object files
#OBJ = $(SRC:%.c=%.o)
#OBJ = fgb.o

# Get a list of the corresponding dependency files
#DEP = $(OBJ:%.o=%.d)

# Include the dependencies here
#-include $(DEP)

# List of libraries to link with
LIBS = $(addprefix -l,rt stdc++)

# Use '@' to hide echoing the actual command
fgb.o: fgb.c
	@ echo [Compile] $<
	@ gcc $(CFLAGS) -c -o $@ $<

# Cannot use '$^' to use all object files
fgb: fgb.o
	gcc -o $@ $(LIBS) fgb.o


.PHONY: all clean
