import "DPI-C" function void breakpoint_trace(input int  stmt_id);

module mod(input logic a, output logic b);

always_comb begin
    breakpoint_trace(0);    
    b = a;
end

endmodule
