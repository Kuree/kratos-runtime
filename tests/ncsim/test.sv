import "DPI-C" function void breakpoint_trace(input int  stmt_id);
module mod (
  input logic [15:0] in1 /*verilator public*/,
  input logic [15:0] in2 /*verilator public*/,
  output logic [15:0] out /*verilator public*/
);

always_comb begin
  breakpoint_trace (32'h0);
  if (in1 == 16'h2) begin
    breakpoint_trace (32'h1);
    out = 16'h2;
  end
  else begin
    breakpoint_trace (32'h2);
    if (in1 == 16'h1) begin
      breakpoint_trace (32'h3);
      out = 16'h0;
    end
    else begin
      breakpoint_trace (32'h4);
      if (in2 == 16'h1) begin
        breakpoint_trace (32'h5);
        out = 16'h1;
      end
      else begin
        breakpoint_trace (32'h6);
        out = 16'h3;
      end
    end
  end
end
endmodule   // mod

