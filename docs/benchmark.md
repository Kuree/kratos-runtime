# Benchmarks
This documentation reports the performance benchmark of various techniques
used in the `kratos-runtime`
## DPI-Based Trap Performance Evaluation
Native design code:
```SystemVerilog
module test(input a, input b, output logic c);
always_comb begin
    c = a + b;
end
endmodule
```

DPI-based breakpoint
```SystemVerilog
import "DPI-C" function void breakpoint(input int i);

module test(input a, input b, output logic c);
always_comb begin
    breakpoint(0);
    c = a + b;
end
endmodule
```

Test benchfile:
```SystemVerilog
module TOP;

logic a, b, c;

test dut (.a(a), .b(b), .c(c));

initial begin
static int i = 0;
for (int ii = 0; ii < 10000; ii++) begin
    a = ii & 1;
    for (int jj = 0; jj < 10000; jj++) begin
        b = jj & 1;
        i += 1;
    end
end
$display("%d", i);
end

endmodule
```
Notice that `i` is used to prevent optimization from the simulator
to remove the tests since there is no observable outcome.

Two variants of DPI call implementation:
1. Empty code block
    ```C++
        void breakpoint(int) {}
    ```
2. Look up
    ```C++
    std::unordered_set<int> breakpoints = {42};
        void breakpoint(int i) {
        if (breakpoints.find(i) != breakpoints.end()) {
            printf("SHALL NEVER HIT\n");
        }
    }
    ```

We compare the performance based on the number of inner loop iteration.
Since `ncsim` also has an option to put breakpoint on any arbitrary
statement (turned on by using `-linedebug`), we also compare the performance
with the vendor implementation. The unit is in seconds.

| Iterations  | Native | DPI_Empty | DPI_LookUp | Line_Debug |
|-------------|--------|-----------|------------|------------|
| 40000000000 | 73     | 85        | 102        | 833        |
| 20000000000 | 36     | 51        | 51         | 478        |
| 10000000000 | 18     | 26        | 26         | 207        |
| 5000000000  | 9.3    | 12.7      | 13.1       | 105        |
| 1000000000  | 1.9    | 2.7       | 2.74       | 21         |

As we can from the table above, after branch prediction warm up, the
DPI with look up is roughly 40% slower than the native execution.
Never the less, it is way faster than (8x) the vendor-based breakpoints.
We believe it is due to the complexity introduced by the `linedebug` switch
that allows arbitrary pause during simulation.