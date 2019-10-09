`timescale 1ns / 1ps
module TOP;

logic[3:0] in;
logic[3:0] out;
logic clk = 0;

parent parent(.*);


always #1 clk = ~clk;

initial begin

    for (int i = 0; i < 4; i++) begin
        in = i;
        #2;
    end
    #2;
    $finish;
end

endmodule
