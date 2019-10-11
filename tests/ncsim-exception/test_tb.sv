`timescale 1ns / 1ps
module TOP;

logic[0:0] in;
logic[0:0] out;

mod mod(.*);

initial begin

    for (int i = 0; i < 4; i++) begin
        in = i;
        #1;
    end

end

endmodule
