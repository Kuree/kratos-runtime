module TOP;

logic [7:0] in;
logic [7:0] out;

parent parent(.*);


initial begin
    int i;
    for (i = 0; i < 10; i++) begin
        in = i;
        #1;
    end

end

endmodule
