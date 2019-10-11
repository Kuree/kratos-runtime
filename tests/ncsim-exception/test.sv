import "DPI-C" function void breakpoint_trace(input int  stmt_id);
import "DPI-C" function void exception(input int  stmt_id);
module mod (
  input logic  in,
  output logic  out
);

always_comb begin
  breakpoint_trace (32'h0);
  out = in - 1'h1;
  breakpoint_trace (32'h1);
  assert (out == in) else exception (32'h1);
end
endmodule   // mod

