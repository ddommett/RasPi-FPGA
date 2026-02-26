// RPi-FPGA-01-FIFO - Raspberry Pi 4 and Lattice ice40HX4K FPGA
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

// This is a design which uses a 64 deep Async FIFO with 9 bits
 
// An example of including a header that defines a macro 'OUTPUT_PINS'
// `include "defines.vh"

// This will generate an ERROR: Undefined wire (or similar) message for any variable
// that is used but not explicitly declared.
`default_nettype none

//******************************************************************************
// Define the 2-stage synchronizer for external signals 
//******************************************************************************
module synchronizer #(parameter WIDTH=3) (input clk, input rst_n, input [WIDTH:0] d_in, output reg [WIDTH:0] d_out);
  reg [WIDTH:0] q1;
  always@(posedge clk) begin
    if(!rst_n) begin
      q1 <= 0;
      d_out <= 0;
    end
    else begin
      q1 <= d_in;
      d_out <= q1;
    end
  end
endmodule

//******************************************************************************
// Define the Asynchronous FIFO
//******************************************************************************
module asynchronous_fifo #(parameter DEPTH=8, DATA_WIDTH=8, ALMOST_THRESHOLD=8) (
  input wclk, wrst_n,
  input rclk, rrst_n,
  input w_en, r_en,
  input [DATA_WIDTH-1:0] data_in,
  output reg [DATA_WIDTH-1:0] data_out,
  output reg full, empty,
  output [DATA_WIDTH-1:0] debug,
  output reg almost_full
);
  
  parameter PTR_WIDTH = $clog2(DEPTH);

  reg [PTR_WIDTH:0] g_wptr_sync, g_rptr_sync;
  reg [PTR_WIDTH:0] b_wptr, b_rptr;
  reg [PTR_WIDTH:0] g_wptr, g_rptr;

  wire [PTR_WIDTH-1:0] waddr, raddr;

  synchronizer #(PTR_WIDTH) sync_wptr (rclk, rrst_n, g_wptr, g_wptr_sync); //write pointer to read clock domain
  synchronizer #(PTR_WIDTH) sync_rptr (wclk, wrst_n, g_rptr, g_rptr_sync); //read pointer to write clock domain 
  
  // ---------- Handle the Write Pointer ----------
  reg [PTR_WIDTH:0] b_wptr_next;
  reg [PTR_WIDTH:0] g_wptr_next;
   
  reg wrap_around;
  wire wfull;
  
  assign b_wptr_next = b_wptr+(w_en & !full);
  assign g_wptr_next = (b_wptr_next >>1)^b_wptr_next;
  
  always@(posedge wclk or negedge wrst_n) begin
    if(!wrst_n) begin
      b_wptr <= 0; 
      g_wptr <= 0;
    end
    else begin
      b_wptr <= b_wptr_next; 
      g_wptr <= g_wptr_next;
    end
  end
  
  always@(posedge wclk or negedge wrst_n) begin
    if(!wrst_n) full <= 0;
    else        full <= wfull;
  end

  assign wfull = (g_wptr_next == {~g_rptr_sync[PTR_WIDTH:PTR_WIDTH-1], g_rptr_sync[PTR_WIDTH-2:0]});

  // ---------- Handle the Read Pointer---------- 
  reg [PTR_WIDTH:0] b_rptr_next;
  reg [PTR_WIDTH:0] g_rptr_next;

  wire rempty;

  assign b_rptr_next = b_rptr+(r_en & !empty);
  assign g_rptr_next = (b_rptr_next >>1)^b_rptr_next;
  assign rempty = (g_wptr_sync == g_rptr_next);
  
  always@(posedge rclk or negedge rrst_n) begin
    if(!rrst_n) begin
      b_rptr <= 0;
      g_rptr <= 0;
    end
    else begin
      b_rptr <= b_rptr_next;
      g_rptr <= g_rptr_next;
    end
  end
  
  always@(posedge rclk or negedge rrst_n) begin
    if(!rrst_n) empty <= 1;
    else        empty <= rempty;
  end

  // ---------- Define the fifo memory block ----------
  reg [DATA_WIDTH-1:0] fifo[0:DEPTH-1];

  always@(posedge wclk) begin
      if(w_en & !full) begin
          fifo[b_wptr[PTR_WIDTH-1:0]] <= data_in;
      end
  end
  assign data_out = fifo[b_rptr[PTR_WIDTH-1:0]];
  assign debug = fifo[1];

  // ---------- Define the rest of the asynchronous fifo ---------- 
  reg[PTR_WIDTH:0] count;
  reg[PTR_WIDTH:0] diff;
  always @ (posedge wclk)
  begin
      if (!wrst_n) begin
          count <= 0;
      end else begin
          if (w_en & !r_en) begin
              count <= count + 1;
          end else begin
              if (!w_en & r_en) begin
                  count <= count - 1;
              end
          end
      end
      diff <= (DEPTH-count);
      almost_full <= (diff < ALMOST_THRESHOLD);
  end
endmodule


//******************************************************************************
// Define the Main Application
//******************************************************************************
    localparam FIFO_DEPTH = 4096; 
    localparam FIFO_WIDTH = 9; 
    localparam FIFO_THRESHOLD = 64; 
module top (
    // An example of using a macro 'OUTPUT_PINS'	
    //	`OUTPUT_PINS

    input r_en_in,    // PI gpio 10, (pin 19)
    input ext_reset_n,    // PI gpio 9, (pin 21)
    input frame_done_in,     // PI gpio 24, (pin 18)
        output reg frame, // PI gpio 11, (pin 23)
        output B1, // Debug
        output B2, // Debug
        output B3, // Debug
        output B4, // Debug
        output B5, // Debug
        output B6, // Debug
        output B7, // Debug
        output B8, // Debug
        output reg[FIFO_WIDTH-1:0] d,

        output A1, // Debug
        output A2, // Debug
        output A3, // Debug
        output A4, // Debug
        output A5, // Debug
        output A6, // Debug
        output A7, // Debug
        output A8, // Debug
        output A9, // Debug

        output reg led1, led2, led3,
        input clk_100mhz
    );

    wire[8:0] AA;
    assign AA = {A9, A8, A7, A6, A5, A4, A3, A2, A1};

    //    assign {d7, d6, d5, d4, d3, d2, d1, d0} = data;

    //------------------------------------------------------------------------------------------
    // Clock Generator
    //------------------------------------------------------------------------------------------
    wire clk;
    assign clk = clk_100mhz;

    //------------------------------------------------------------------------------------------
    // Synchronize all asynchronous inputs (reset has its own synchronizer)
    //------------------------------------------------------------------------------------------
    wire frame_done;
    synchronizer #(1) sync_frame_done (clk, resetn, frame_done_in, frame_done);

    //------------------------------------------------------------------------------------------
    // Reset - go through metastability flip flops
    //------------------------------------------------------------------------------------------
    reg resetn = 0;
    reg resetn1 = 0;
    reg resetn2 = 0;
    always @(posedge clk) begin
        resetn1 <= ext_reset_n;
        resetn2 <= resetn1;
        resetn <= resetn2;
    end

    always @(posedge clk) begin
//        d <= 8'b10100101; // 0xA5
//        d <= counterDummyData[7:0];
        d <= data_out;
    end

    //------------------------------------------------------------------------------------------
    // Heartbeat
    // The heartbeat goes if we have a clock (unaffected by the reset line)
    //------------------------------------------------------------------------------------------
    parameter COUNTER_MAX = 10_000_000; 

    reg[31:0] counterHB; // 32-bit counter register

    always @(posedge clk) begin
        if (counterHB == COUNTER_MAX - 1) begin 
            counterHB <= 0; // Reset counter
            led3 <= ~led3; // Toggle the LED state
        end else begin
            counterHB <= counterHB + 1; // Increment counter
        end
    end

    //------------------------------------------------------------------------------------------
    // Other LEDs
    // LED2 is half-lit when out of reset
    //------------------------------------------------------------------------------------------
    always @(posedge clk) begin
        led1 <= 0;
        if (!resetn)  begin
            led2 <= 0;
        end else  begin
            led2 <= ~led2;
        end
    end

    //------------------------------------------------------------------------------------------
    // The state machine
    //------------------------------------------------------------------------------------------
    parameter START = 0, COUNTING = 1, PULSE = 2, DONE = 3;

    reg[1:0] state, next;
    reg[15:0] counter50k; 
    reg go;

    // Sequential block to change state on clock edge
    always @ (posedge clk) 
    begin
        if (!resetn) begin
            state <= START;
            go <= 1'b0;
        end else begin
            case (state)
                START: begin
                    counter50k <= 0;
                    go <= 1'b0;
                    state <= COUNTING;
                end

                COUNTING: begin
                    go <= 1'b0;
                    counter50k <= counter50k + 1;
                    if (counter50k >= 2000)
                        state <= PULSE; 
                end

                PULSE: begin
                    go <= 1'b1;
                    counter50k <= 0;
                    state <= DONE; 
                end 

                DONE: begin  
                go <= 1'b0;
                counter50k <= 0;
                state <= START;
            end 

        endcase
    end
end

// We use @* for combinatorial logic
//always @* begin
//end

//-----------------------------------------------------------------------------
// The detectors state machine
//-----------------------------------------------------------------------------
typedef enum logic [3:0] {
    B_START,    
    B_WRITE, 
    B_WRITE2, 
    B_WRITE3, 
    B_WAIT_DONE, 
    B_WAIT_DONE2, 
    B_WAIT_DONE3, 
    B_ERROR,
    B_DONE,
  } type_stateB;
type_stateB stateB; 

reg[31:0] counterDummyData; 
reg[15:0] countBytes; 
wire[FIFO_WIDTH-1:0] data_out; 
reg wrst_n, full, empty, w_en;
wire[FIFO_WIDTH-1:0] debug;
wire almost_full;

    //asynchronous_fifo #(FIFO_DEPTH, FIFO_WIDTH) fifo (wclk, wrst_n, rclk, rrst_n, w_en, r_en,
    //    data_in, data_out, full, empty);
    //asynchronous_fifo #(FIFO_DEPTH, FIFO_WIDTH) fifo (clk, wrst_n, clk, wrst_n, w_en, r_en2,
    //    counterDummyData[7:0], data_out, full, empty, debug);
    asynchronous_fifo #(FIFO_DEPTH, FIFO_WIDTH, FIFO_THRESHOLD) fifo (clk, wrst_n, clk, wrst_n, w_en, r_en2,
        genData[FIFO_WIDTH-1:0], data_out, full, empty, debug, almost_full);

//    Synchronous_FIFO #(FIFO_WIDTH, FIFO_DEPTH) fifo (clk, wrst_n, genData[FIFO_WIDTH-1:0], 
//        w_en, r_en2, data_out, full, empty);

  //reg [7:0] fifoA[0:64-1];
//reg[FIFO_WIDTH-1:0] headData; 
reg[15:0] headData; 

// Sequential block to change state on clock edge
always @ (posedge clk) 
begin
    if (!resetn) begin
        stateB <= B_START;
        counterDummyData <= 0;
        headData <= 0;
        wrst_n <= 0;
        w_en <= 0;
        frame <= 0;
    end else begin
        frame <= !empty;
        if (go == 1) begin
            headData <= headData + 1;
        end
        case (stateB)
            B_START: begin
                countBytes <= 0;
                counterDummyData <= 0;
                wrst_n <= 1;
                w_en <= 0;
                //if ((go == 1) && (full == 0) && (rrst_n == 0)) begin
                //if ((full == 0) && (!frame_done)) begin
                //if ((empty) && (go == 1)) begin
                if ((!almost_full) && (go == 1)) begin
                    stateB <= B_WRITE;
                end
            end

            B_WRITE: begin
                //fifoA[countBytes] <= countBytes;
                w_en <= 1;
                stateB <= B_WRITE2;
            end

            B_WRITE2: begin
                w_en <= 0;
                counterDummyData <= counterDummyData + 1;
                countBytes <= countBytes + 1;
                stateB <= B_WRITE3;
            end

            B_WRITE3: begin
                //if (full == 1) begin
                if (countBytes >= 64) begin
//                    frame <= 1;
//                    stateB <= B_WAIT_DONE;
                    stateB <= B_DONE;
                end else begin
                    stateB <= B_WRITE;
                end
            end

            B_WAIT_DONE: begin
                if (frame_done) begin
//                    frame <= 0;
                    stateB <= B_WAIT_DONE2;
                end
            end

            B_WAIT_DONE2: begin
                if (!frame_done) begin
//                        frame <= 0;
                        stateB <= B_WAIT_DONE3;
                end
            end

            B_WAIT_DONE3: begin
                    stateB <= B_DONE;
            end

            B_ERROR: begin
                stateB <= B_ERROR;
            end

            B_DONE: begin
//                if (!empty) begin
//                    stateB <= B_ERROR;
//                end else begin
                begin
                    stateB <= B_START;
                end
            end

        default: 
            stateB <= B_START;
    endcase
end
end

typedef enum logic [2:0] {
    C_START,    
    C_DONE,
  } type_stateC;
type_stateC stateC; 

//-----------------------------------------------------------------------------
// Generate a pulse on each edge of r_en
//-----------------------------------------------------------------------------
    //wire r_en;
    //synchronizer #(1) sync_r_en (clk, resetn, r_en_in, r_en);
reg r_enA, r_enB, r_enC, r_en2;
always @ (posedge clk) 
begin
    if (!resetn) begin
        r_enA <= 0;
        r_enB <= 0;
        r_enC <= 0;
        r_en2 <= 0;
    end else begin
        r_enA <= r_en_in;
        r_enB <= r_enA;
        r_enC <= r_enB;
        r_en2 <= (r_enC != r_enB);
        //r_enC <= r_en;
        //r_en2 <= (r_enC != r_en);
    end
end

always @ (posedge clk) 
begin
    if (!resetn) begin
        stateC <= C_START;
    end else begin
        case (stateC)
            C_START: begin
                if (r_en2 == 1) begin
                    stateC <= C_DONE;
                end
            end

            C_DONE: begin
                stateC <= C_START;
            end

        default: 
            stateC <= C_START;
    endcase
end
end

//-----------------------------------------------------------------------------
// The data generator state machine
//-----------------------------------------------------------------------------
typedef enum logic [3:0] {
    D_START,    
    D_HEADER1,
    D_HEADER2,
    D_HEADER3,
    D_PACKET1,
    D_DONE
  } type_stateD;
type_stateD stateD; 

reg[FIFO_WIDTH-1:0] packetData; 
wire[FIFO_WIDTH-1:0] genData; 

/*always @(*)
begin
    if (!resetn) begin
        genData = headData;
    end
end*/

always @ (posedge clk) 
begin
    if (!resetn) begin
        stateD <= D_START;
        //genData = headData;
        genData = {1'b0, headData[7:0]};
        packetData <= 0;
    end else begin
        case (stateD)
            D_START: begin
                genData <= 8'b10100101;
                //genData <= headData;
                packetData <= 0;
                if (stateB != B_START) begin
                    stateD <= D_HEADER1;
                end
            end

            D_HEADER1: begin
                genData <= {1'b0, headData[7:0]};
                if (stateB == B_WRITE2) begin
                    stateD <= D_HEADER2;
                end 
            end

            D_HEADER2: begin
                //genData <= headData;
                genData <= {1'b0, headData[7:0]};
                if (stateB == B_WRITE2) begin
//                    headData <= headData + 1;
                    stateD <= D_HEADER3;
                end 
            end

            D_HEADER3: begin
                //genData <= headData;
                genData <= {1'b0, headData[15:8]};
                if (stateB == B_WRITE2) begin
//                    headData <= headData + 1;
                    stateD <= D_PACKET1;
                end 
            end

            D_PACKET1: begin
                genData <= packetData;
                if (stateB == B_WRITE2) begin
                    if (countBytes >= 62) begin
                        packetData <= 9'b111111111;
                    end else begin
                        if (countBytes >= 60) begin
                            packetData <= 9'b100000000;
                        end else begin
                            packetData <= packetData + 1;
                        end
                    end
                end 
                //if (stateB == B_WAIT_DONE) begin
                if (stateB == B_DONE) begin
                    stateD <= D_DONE;
                end 
            end

            D_DONE: begin
                genData <= packetData;
//                if (stateB == B_DONE) begin
                    stateD <= D_START;
//                end
            end

        default: 
            stateD <= D_START;
    endcase
end
end

//-----------------------------------------------------------------------------
// Debugging
//-----------------------------------------------------------------------------
    assign B1 = go;
    assign B2 = resetn;

    assign B3 = frame;
    //assign B4 = ((stateB != B_START) && go) ? 1 : 0;
    assign B4 = r_enB;
    assign B5 = frame_done;
    assign B6 = empty;
    assign B7 = almost_full;
    //assign B8 = (stateB == B_ERROR) ? 1 : 0;
    assign B8 = full;
    //always @* begin
    //end

    assign AA = headData;
/*    assign A1 = headData[0];
    assign A2 = headData[1];
    assign A3 = headData[2];
    assign A4 = headData[3];
    assign A5 = headData[4];
    assign A6 = headData[5];
    assign A7 = headData[6];
    assign A8 = headData[7];*/

endmodule

