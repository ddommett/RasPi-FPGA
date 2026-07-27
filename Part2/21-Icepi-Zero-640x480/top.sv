// Icepi-Zero-640x480 - Raspberry Pi 5 and Lattice ECP5 FPGA
// Copyright (C) 2026  David Dommett, email: david.dommett@gmail.com 

// Credit to Cheyao at https://github.com/cheyao/icepi-zero for most
// of this source

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
`timescale 10ns/10ns // 50MHz = 20ns perios for clk = 10ns per change

//*******************************************************************************************
// Produce a dummy HDMI signal at 640x480
//*******************************************************************************************
module top (
	input  wire        clk,
	input  wire  [1:0] button,
	output logic [3:0] gpdi_dp,
	output logic [4:0] led
);

	// 640x480 @ 60Hz
	localparam HA_END = 639;
	localparam HS_STA = HA_END + 16;
	localparam HS_END = HS_STA + 96;
	localparam LINE   = 799;

	localparam VA_END = 479;
	localparam VS_STA = VA_END + 10;
	localparam VS_END = VS_STA + 2; 
	localparam SCREEN = 524;

	wire        vsync, hsync, de;
	wire  [7:0] vga_r, vga_g, vga_b;
	wire  [23:0] frame;
	wire  [23:0] seqID;

	// pixel and tmds clock (25MHz and 250MHz)
	wire clkp, clkt;

	// VGA input and output signals
	logic [7:0] r, g, b;
	wire  [9:0] px, py;
	logic [7:0] offset;

	// vga signals to tmds 
	logic [3:0] counter;
	logic       load;
	logic [9:0] rt, gt, bt;
	logic [9:0] rts, gts, bts;

	assign gpdi_dp[0] = bts[0]; // The output pins on the digital video port
	assign gpdi_dp[1] = gts[0];
	assign gpdi_dp[2] = rts[0];
	assign gpdi_dp[3] = clkp;   

	//*******************************************************************************************
	//*******************************************************************************************
	//*******************************************************************************************

    	initial begin
        	px = 0;
        	py = 0;
		frame = 0;
		seqID = 0;
    	end

	//*******************************************************************************************
	// Generate the pixel clock at 25MHz 
	// Generate the tmds clock at 250MHz 
	//*******************************************************************************************
	(* FREQUENCY_PIN_CLKI="50" *)
	(* FREQUENCY_PIN_CLKOP="250" *)
	(* FREQUENCY_PIN_CLKOS="25" *)

	(* ICP_CURRENT="12" *) (* LPF_RESISTOR="8" *) (* MFG_ENABLE_FILTEROPAMP="1" *) (* MFG_GMCREF_SEL="2" *)
	/* verilator lint_off PINCONNECTEMPTY */
	EHXPLLL #(
		.PLLRST_ENA("DISABLED"),
		.INTFB_WAKE("DISABLED"),
		.STDBY_ENABLE("DISABLED"),
		.DPHASE_SOURCE("DISABLED"),
		.OUTDIVIDER_MUXA("DIVA"),
		.OUTDIVIDER_MUXB("DIVB"),
		.OUTDIVIDER_MUXC("DIVC"),
		.OUTDIVIDER_MUXD("DIVD"),
		.CLKI_DIV(12),
		.CLKOP_ENABLE("ENABLED"),
		.CLKOP_DIV(2),
		.CLKOP_CPHASE(0),
		.CLKOP_FPHASE(0),
		.CLKOS_ENABLE("ENABLED"),
		.CLKOS_DIV(20),
		.CLKOS_CPHASE(0),
		.CLKOS_FPHASE(0),
		.FEEDBK_PATH("CLKOP"),
		.CLKFB_DIV(60)
		) pll_i (
			.RST(1'b0),
			.STDBY(1'b0),
			.CLKI(clk),
			.CLKOP(clkt),
			.CLKOS(clkp),
			.CLKFB(clkt),
			.CLKINTFB(),
			.PHASESEL0(1'b0),
			.PHASESEL1(1'b0),
			.PHASEDIR(1'b1),
			.PHASESTEP(1'b1),
			.PHASELOADREG(1'b1),
			.PLLWAKESYNC(1'b0),
			.ENCLKOP(1'b0),
			.LOCK(locked)
		);

	// Output colors
	always_comb begin
//		r = py[7:0] + offset;
//		g = px[7:0] + offset;
//		b = px[7:0] + offset;
		if (px == 300) begin
			r = seqID[7:0];
			g = seqID[15:8];
			b = seqID[23:16];
		end else begin
			r = frame[7:0];
			g = frame[15:8];
			b = frame[23:16];
		end

		// Generate hsync and vsync signals
		hsync = ~(px >= HS_STA && px < HS_END);
		vsync = ~(py >= VS_STA && py < VS_END);
		de = (px <= HA_END && py <= VA_END);

		vga_r = de ? r : 8'b0;
		vga_g = de ? g : 8'b0;
		vga_b = de ? b : 8'b0;
	end

	// On every pixel clock, px moves to the next pixel.
	// If px reaches the end of a line, 
	// 	- px	is reset to 0
	// 	- py moves to next line
	// 	If py has reached the end of the screen,
	// 		- py is reset to 0
	// Whenever we reach the top left of the screen, the "offset" is
	// incremented so that the screen "drifts"
	always @(posedge clkp) begin
 


		if (px == 0 && py == 0) begin

			seqID <= seqID + 1;
			
			if (frame < 256) begin
				frame <= frame + 1;
			end else begin
				if (frame <= 'hFFFF) begin
					frame <= frame + 256;
				end else begin
					frame <= frame + 'h010000;
				end
			end
			offset <= offset + 1;
		end

		// Count the x, y positions
		if (px == LINE) begin
			px <= 0;
			py <= (py == SCREEN) ? 0 : py + 1;
		end else begin
			px <= px + 1;
		end
    	end
    
	// On every tmds clock, 
	// - a counter is increased (it wraps after 10 counts)
	// - the "load" pulse fires on the 10th bit
	// - the xts (rgb) registers load from xt (rgb) registers when load
	//   is true, else they shift right 
	// In this manner, the r,g,b values are serially clocked out with the 
	// tmds clock which is 10x the pixel clock
	//
	// rt, gt, bt come from the tmds encoder which converts the 8-bit
	// values into a 10-bit symbol
	always_ff @(posedge clkt) begin
		load <= (counter == 4'd9);

		rts <= load ? rt : {1'b0, rts[9:1]};
		gts <= load ? gt : {1'b0, gts[9:1]};
		bts <= load ? bt : {1'b0, bts[9:1]};

		counter <= (counter == 4'd9) ? 4'b0 : counter + 1;
	end

    	assign led = {hsync, vsync, de, button};


//*******************************************************************************************
// Convert vga signals to tmds https://www.fpga4fun.com/HDMI.html
//*******************************************************************************************

	// Encode tmds signals
	tmds_encoder B_encode (clkp, vga_b, {vsync, hsync}, de, bt);
	tmds_encoder G_encode (clkp, vga_g, {2'b0},         de, gt);
	tmds_encoder R_encode (clkp, vga_r, {2'b0},         de, rt);




endmodule
