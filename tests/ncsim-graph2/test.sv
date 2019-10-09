import "DPI-C" function void breakpoint_clock();
import "DPI-C" function void breakpoint_trace(input int  stmt_id);
module child (
  input logic  clk,
  input logic [3:0] in,
  output logic [3:0] out
);


always_ff @(posedge clk) begin
  out <= in;
  breakpoint_trace (32'h1);
end
endmodule   // child

module child_unq0 (
  input logic  clk,
  input logic [3:0] in,
  output logic [3:0] out
);


always_ff @(posedge clk) begin
  out <= in;
end
endmodule   // child_unq0

module mod (
  input logic  clk,
  input logic [3:0] in,
  output logic [3:0] out
);

logic  [3:0] child0_out;
logic  [3:0] child1_out;
logic  [3:0] child2_out;

always_ff @(posedge clk) begin
  out <= child0_out + child1_out + child2_out;
  breakpoint_trace (32'h9);
end
child child0 (
  .clk(clk),
  .in(in),
  .out(child0_out)
);

child child1 (
  .clk(clk),
  .in(child0_out),
  .out(child1_out)
);

child_unq0 child2 (
  .clk(clk),
  .in(child1_out),
  .out(child2_out)
);

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

