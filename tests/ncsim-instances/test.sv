import "DPI-C" function void breakpoint_trace(input int instance_id, input int stmt_id);
module child_8_2 #(parameter KRATOS_INSTANCE_ID = 32'h2)
(
  input logic [7:0] child_in,
  output logic [7:0] child_out
);

always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h4);
  child_out = 8'h0;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h5);
  child_out = child_in + 8'h2;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h6);
  child_out = child_in + 8'h2;
end
endmodule   // child_8_2

module child_8_3 #(parameter KRATOS_INSTANCE_ID = 32'h0)
(
  input logic [7:0] child_in,
  output logic [7:0] child_out
);

always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h7);
  child_out = 8'h0;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h8);
  child_out = child_in + 8'h3;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h9);
  child_out = child_in + 8'h3;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'hA);
  child_out = child_in + 8'h3;
end
endmodule   // child_8_3

module child_8_4 #(parameter KRATOS_INSTANCE_ID = 32'h1)
(
  input logic [7:0] child_in,
  output logic [7:0] child_out
);

always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'hB);
  child_out = 8'h0;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'hC);
  child_out = child_in + 8'h4;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'hD);
  child_out = child_in + 8'h4;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'hE);
  child_out = child_in + 8'h4;
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'hF);
  child_out = child_in + 8'h4;
end
endmodule   // child_8_4

module parent #(parameter KRATOS_INSTANCE_ID = 32'h3)
(
  input logic [7:0] in,
  output logic [7:0] out
);

logic [7:0] child_0_child_out;
logic [7:0] child_1_child_out;
logic [7:0] child_2_child_out;
always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h0);
  out = child_0_child_out + child_1_child_out + child_2_child_out;
end
always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h1);
end
always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h2);
end
always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h3);
end
child_8_2 #(
  .KRATOS_INSTANCE_ID(32'h2)) child_0 (
  .child_in(in),
  .child_out(child_0_child_out)
);

child_8_3 #(
  .KRATOS_INSTANCE_ID(32'h0)) child_1 (
  .child_in(in),
  .child_out(child_1_child_out)
);

child_8_4 #(
  .KRATOS_INSTANCE_ID(32'h1)) child_2 (
  .child_in(in),
  .child_out(child_2_child_out)
);

endmodule   // parent

