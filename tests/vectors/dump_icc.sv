module mod #(parameter KRATOS_INSTANCE_ID = 32'h0)
(
  input logic [3:0] in,
  output logic [3:0] out,
  input logic [3:0] sel
);

always_comb begin
  if (sel) begin
    out = 4'h0;
  end
  else begin
    out[0] = 1'h1;
    out[1] = 1'h1;
    out[2] = 1'h1;
    out[3] = 1'h1;
  end
end
endmodule   // mod

