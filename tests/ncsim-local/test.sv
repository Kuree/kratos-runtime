import "DPI-C" function void breakpoint_trace(input int  stmt_id);
module mod (
  input logic [3:0] in,
  output logic [3:0] out,
  input logic [3:0] sel
);

always_comb begin
  breakpoint_trace (32'h0);
  if (sel) begin
    breakpoint_trace (32'h1);
    out = 4'h0;
  end
  else begin
    breakpoint_trace (32'h2);
    out[0] = 1'h1;
    breakpoint_trace (32'h3);
    out[1] = 1'h1;
    breakpoint_trace (32'h4);
    out[2] = 1'h1;
    breakpoint_trace (32'h5);
    out[3] = 1'h1;
  end
end
endmodule   // mod

