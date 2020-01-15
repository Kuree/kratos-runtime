module TOP;

logic[15:0] in1;
logic[15:0] in2;
logic[15:0] out;

mod mod(.*);

initial begin

    for (int i = 0; i < 4; i++) begin
        in1 = i;
        #1; in2 = i;
        #1;
    end

end

endmodule
