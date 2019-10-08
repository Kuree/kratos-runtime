import "DPI-C" function void breakpoint_clock();
import "DPI-C" function void breakpoint_trace(input int  stmt_id);
module mod (
  input logic  clk,
  input logic [3:0] in,
  output logic [3:0] out
);


always_ff @(posedge clk) begin
  out <= in;
  breakpoint_trace (32'h3);
end
endmodule   // mod

module parent (
  input logic  clk,
  input logic [3:0] in,
  output logic [3:0] out
);

logic  [3:0] mod0_out;
logic  [3:0] mod1_out;
logic  [3:0] mod2_out;

always_ff @(negedge clk) begin
  breakpoint_clock ();
end
mod mod0 (
  .clk(clk),
  .in(in),
  .out(mod0_out)
);

mod mod1 (
  .clk(clk),
  .in(mod0_out),
  .out(mod1_out)
);

mod mod2 (
  .clk(clk),
  .in(mod1_out),
  .out(mod2_out)
);

mod mod3 (
  .clk(clk),
  .in(mod2_out),
  .out(out)
);

endmodule   // parent

