module TOP;

logic[3:0] in;
logic[3:0] sel;
logic[3:0] out;

mod mod(.*);

initial begin

    for (int i = 0; i < 4; i++) begin
        in = i;
        #1; sel = i;
        #1;
    end

end

endmodule
