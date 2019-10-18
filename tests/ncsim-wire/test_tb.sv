`timescale 1ns / 1ps
module TOP;

logic in;
logic out;

mod mod(.*);

initial begin

    for (int i = 0; i < 4; i++) begin
        in = i;
        #1;
    end
end

endmodule
