// RPi-FPGA-01-Blinky-Example - Raspberry Pi 4 and Lattice ice40HX4K FPGA
// Copyright (C) 2026  David Dommett, email: david.dommett@gmail.com 

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
`default_nettype none

module top (
        output reg led1, led2, led3, led4,
        input clk_100mhz
    );

    wire clk;
    assign clk = clk_100mhz;

    parameter COUNTER_MAX = 20_000_000; 

    reg[31:0] counterHB; // 32-bit counter register

    always @(posedge clk) begin
        if (counterHB == COUNTER_MAX - 1) begin 
            counterHB <= 0; // Reset counter
            led1 <= ~led1; // Toggle the LED state
        end else begin
            counterHB <= counterHB + 1; // Increment counter
        end
    end

endmodule

