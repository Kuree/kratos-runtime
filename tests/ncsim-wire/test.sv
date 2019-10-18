import "DPI-C" function void breakpoint_trace(input int  stmt_id);
module mod (
  input logic  in,
  output logic  out
);

always_comb begin
  breakpoint_trace (32'h0);
  out = in;
end
endmodule   // mod

