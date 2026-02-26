
all: dt 
	echo Built Device Tree Overlay

dt: fast_gpio_bus_overlay.dts
	dtc -@ -I dts -O dtb -o fast_gpio_bus_overlay.dtbo fast_gpio_bus_overlay.dts

clean:
	rm -rf fast_gpio_bus_overlay.dtbo
	
.PHONY: all clean
