VERILOG_FILES := tmds_encoder.sv top.sv 

OUTPUT := bitstream
PACKAGE := CABGA256

all: debug

%.json: ${VERILOG_FILES}
	rm -f ${OUTPUT}.bit ${OUTPUT}.config ${OUTPUT}.json
	yosys -p 'synth_ecp5 -top top -json $@' $^

%.config: %.json icepi-zero-v1_3.lpf
	nextpnr-ecp5 --25k --package ${PACKAGE} --lpf icepi-zero-v1_3.lpf --json $< --textcfg $@ # 2> log.txt
	# cat log.txt | grep Device -A 28

%.bit: %.config
	ecppack --compress $< $@

build: ${OUTPUT}.bit

debug: build
	openFPGALoader -b icepi-zero ${OUTPUT}.bit

clean:
	rm -f $(OUTPUT).bit $(OUTPUT).config $(OUTPUT).json

.PHONY: build clean install sim lint help
