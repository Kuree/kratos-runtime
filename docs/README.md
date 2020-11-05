Kratos-Runtime Debug Database
=============================
This documentation describes the full design details of the `kratos-runtime` as
well as the capability of reference debugger implemented in Visual Studio Code.

## Database Format
Kratos-runtime uses `SQLite3` as storage format. This design choice is made due
to the fact that `SQLite3` is one of the most used database and doesn't depend
on a running service. There are hundreds of language drivers and visualization
frameworks to help users understand the database content.

### Database schema
![Database schema diagram](schema.svg)

Below is the raw SQL query that can be used to create each table
- `instance`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'instance' ( 'id' INTEGER PRIMARY KEY NOT NULL , 'handle_name' TEXT NOT NULL );
    ```
- `hierarchy`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'hierarchy' ( 'parent_handle' INTEGER , 'name' TEXT NOT NULL , 'handle' INTEGER , FOREIGN KEY( parent_handle ) REFERENCES instance ( id ) );
    ```
- `variable`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'variable' ( 'id' INTEGER PRIMARY KEY NOT NULL , 'handle' INTEGER , 'value' TEXT NOT NULL , 'is_verilog_var' INTEGER NOT NULL , FOREIGN KEY( handle ) REFERENCES instance ( id ) );
    ```
- `generator_variable`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'generator_variable' ( 'variable_id' INTEGER , 'handle' INTEGER , 'name' TEXT NOT NULL , FOREIGN KEY( variable_id ) REFERENCES variable ( id ) , FOREIGN KEY( handle ) REFERENCES instance ( id ) );
    ```

- `context`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'context' ( 'variable_id' INTEGER , 'breakpoint_id' INTEGER , 'name' TEXT NOT NULL , FOREIGN KEY( variable_id ) REFERENCES variable ( id ) , FOREIGN KEY( breakpoint_id ) REFERENCES breakpoint ( id ) );
    ```
- `connection`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'connection' ( 'handle_from' INTEGER , 'var_from' TEXT NOT NULL , 'handle_to' INTEGER , 'var_to' TEXT NOT NULL , FOREIGN KEY( handle_from ) REFERENCES instance ( id ) , FOREIGN KEY( handle_to ) REFERENCES instance ( id ) );
    ```
- `breakpoint`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'breakpoint' ( 'id' INTEGER PRIMARY KEY NOT NULL , 'filename' TEXT NOT NULL , 'line_num' INTEGER NOT NULL );
    ```
- `metadata`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'metadata' ( 'name' TEXT NOT NULL , 'value' TEXT NOT NULL );
    ```
- `instance_set`
    ```SQL
    CREATE TABLE IF NOT EXISTS 'instance_set' ( 'instance_id' INTEGER , 'breakpoint_id' INTEGER , FOREIGN KEY( instance_id ) REFERENCES instance ( id ) , FOREIGN KEY( breakpoint_id ) REFERENCES breakpoint ( id ) );
    ```

### Required Table and Extension Table
Some tables are required and some are optional (used for visualization extensions). The minimal table you need to fill out is:
- `breakpoint`
- `instance`
- `variable`

To support context variables (e.g. local frame), you need to fill out the following tables:
- `context`

To support generator variables (e.g. RTL signals), you need to fill out the following tables:
- `generator_variable`

To support data-wave visualization tool, you need to fill out
- `hierarchy`
- `connection`
- `instance_set`

### Database Description and Terminology
Here are some terms used in this documentation
- `handle_name`:

    The `handle_name` is the *full path* of an instance excluding the test bench
    name, such as `TOP`. If your design is named `dut` inside `TOP`, then the top level
    handle name is `dut`. By default the test bench name will be `TOP`, and there is a REST API to change that.

#### `variable` Table

- `id`

    Each variable should have a unique ID. Variables can be linked to context variable to avoid storage overhead.

- `handle`

    Instance that owns the variable. It is a foreign key that links to the instance table.

- `is_verilog_var`

    Boolean value to indicate whether the variable has RTL correspondence or not.

- `value`

    If the variable is has correspondence to RTL signal, the value is the signal name scoped to the instance. As a result, we don't need full path When querying values, we will prefix it with the linked instance `handle_name` and construct full path handle name for the variable.

    If the variable is not a RTL signal i.e. `is_verilog_var` is set to 0, then the value would be the string value of the variable.


#### `breakpoint` Table
- `id`

    Each breakpoint should have a unique ID. Notice that many instances may share the same definition. To allow break on each instance, instance id is also used to as an identifier.

- `filename` and `line_num`

    The `filename` in breakpoint is the *absolute path* to ensure the
    support for remote debugging. `line_num` starts from `1`, as commonly used in text
    editors. The runtime library supports source files remapping, which can be configured through a REST API.


#### `instance` Table

- `id` for `instance`

    Each instance of the design should have a unique ID. See DPI section for more details on how it is used.

- `handle_name`

    This is the full path excluding the top test bench name.

#### `context` Table

- `variable_id`

    Foreign key linked to `variable` `id`. The same variable can be linked by multiple `context` table entries for storage optimization. However, compilers are free to duplicate variables if it is easier to implement (at the cost of storage space).

- `breakpoint_id`

    Foreign key linked to `breakpoint` `id`. Whenever a breakpoint is hit, the runtime will select all context rows that share the same `breakpoint_id` and then construct the context frame.

- `name`

    The name shown in the debugger.


#### `generator_variable` Table

- `variable_id`

    Foreign key linked to `variable` `id`. The same variable can be linked by multiple `context` table entries for storage optimization. However, compilers are free to duplicate variables 

- `handle`

    Instance that owns the variable. It is a foreign key that links to the instance table.

- `name`

    The name shown in the debugger.


#### `connection` Table
Connection table is used to construct connection graph. This represents the RTL-level connectivity. This table may be phased away as we get more supports from the simulator vendors.

#### `instance_set`
This table is used for inspecting concurrent threads in the simulation. When the circuit is in a synchronous state, we can inspect every frame of the circuit design.


## Variable Mapping to Kratos-Runtime
Every variable stored in the database has to be a scalar value. That means if you
have a complex type, such as packed struct or 2D array, you need to flatten the
variable hierarchy. If done in a particular way, the reference debugger can
reconstruct the variable hierarchy.

The flattening process involves adding `.` to the variable hierarchy. For instance,
image you have an object named `obj` defined as follows:
```
obj: {
    value1: port_1,
    value2: port_2
}
```

Where `port_1` and `port_2` correspond to the verilog value. You should store your
variable in the following format

| name       | value  |
| -----------|------- |
| obj.value1 | port_1 |
| obj.value2 | port_2 |

We can use `obj.value1` as `name` when storing data in `context` and `generator_variable` table.

The same rule applies to the verilog value. If you have an array that mapped
to an array in verilog:

```
array -> port_array
```
Since `port_array` is not a scalar, you have to flatten it as following

| name      | value         |
| ----------|-------------- |
| array.0   | port_array[0] |
| array.1   | port_array[1] |


If you have multiple hierarchy either in your own variable or in verilog variable
you can keep adding `.` to build up your hierarchy as long as the `value` is a valid
verilog handle name. The reference debugger will reconstruct the original variable
in the debugger.

## Breakpoint Insertion
The breakpoint is implemented as a `DPI` function. In cases where your hardware
construction language can only generate Verilog-95 or Verilog-01, you have
to somehow manage to generate the following SystemVerilog syntax:
```SystemVerilog
import "DPI-C" function void breakpoint_trace(input int instance_id, input int stmt_id);
```
You should put the `breakpoint_trace(instance_id, id)` call above any statement that has a corresponding line to the original source code.
- `instance_id` is a unique identifier for each instance in the design you want to debug. So if two instances share the same module definition, they still have two different IDs and thus the function arguments will be different. To avoid uniquification, we recommend to implement `instance_id` as a module parameter, as shown in the example.
- `id` is the breakpoint id. It establish the mapping between the source code line and the simulation environment.

You can only put breakpoint statement in the `always` block due to the limitation of DPI function calls. As a result, you have to wrap every continuous assignment into an `always_comb` block.

## Examples
In this section we will use a simple example in Python to illustrate how to generate the database. Although the example is written in `kratos`, the should be transferrable to all hardware generator frameworks.

```Python
class ExampleGenerator(Generator):
    def __init__(self, width=8, add_always=True):
        super().__init__("ExampleGenerator")
        self.a = self.input("a", width)
        self.b = self.output("b", width)
        self.input("c", width)

        @always_comb
        def code_block():
            self.b = self.a & self.ports.c

        if add_always:
            self.add_always(code_block)
```

Here is the SystemVerilog output without breakpoint functions inserted
```SystemVerilog
module ExampleGenerator (
  input logic [7:0] a,
  input logic [7:0] c,
  output logic [7:0] b
);

always_comb begin
  b = a & c;
end
endmodule   // ExampleGenerator
```

Here is the SystemVerilog after breakpoint insertion:
```SystemVerilog
import "DPI-C" function void breakpoint_trace(input int instance_id, input int stmt_id);
module ExampleGenerator #(
  parameter KRATOS_INSTANCE_ID = 32'h0
)
(
  input logic [7:0] a,
  input logic [7:0] c,
  output logic [7:0] b
);

always_comb begin
  breakpoint_trace (KRATOS_INSTANCE_ID, 32'h0);
  b = a & c;
end
endmodule   // ExampleGenerator
```

Notice that proper DPI import is placed at the top of SystemVerilog code. We assume that this module as instantiated as `dut` in the test bench.

### Table Values
To simplify the illustration, we assume there is no optimization on debug information. As a result, we will have duplicated information in the `variable` table. Such duplication will not affect the runtime, as long as the reference is correct.

Since there is only one instance, i.e. `ExampleGenerator`, we only have one row in the `instance` table:

| id |   handle_name    |
| -- | ---------------- |
| 0  | ExampleGenerator |

Similarly, since there is only one breakpoint, we also have one rwo in the `breakpoint` table:


| id | filename        | line_num |
| -- | --------------- | -------- |
| 0  | /tmp/example.py | 13       |

Notice that the line number exactly match line `self.b = self.a & self.ports.c` in Python code.


Since we have 3 RTL signals, we have the following rows in table `generator_variable`:

| variable_id | handle | name |
| ----------- | ------ | ---- |
| 0           | 0      | a    |
| 1           | 0      | b    |
| 2           | 0      | c    |


Notice that `handle` column refers to the instance, in this case, the top module `ExampleGenerator`, as indicated by the `id`.

In kratos, `@always_comb` allows code block accessing caller's local scope. In this case, we have the following context variables:

- `width`
- `add_always`
- `self`

AS a result, we have the following `context` rows:

| variable_id | breakpoint_id | name       |
| ----------- | ------------- | ---------- |
| 3           | 0             | add_always |
| 4           | 0             | width      |
| 5           | 0             | self.a     |
| 6           | 0             | self.b     |


Notice that `breakpoint_id` refers to the `id` in the `breakpoint` table.

For the `variable` table, we have:

| id | handle | value | is_verilog_var |
| -- | ------ | ----- | -------------- |
| 0  | 0      | dut.a | 1              |
| 1  | 0      | dut.b | 1              |
| 2  | 0      | dut.c | 1              |
| 3  | 0      | True  | 0              |
| 4  | 0      | 8     | 0              |
| 5  | 0      | dut.a | 1              |
| 6  | 0      | dut.b | 1              |

For context variable such as `width` and `add_always`, whose IDs are `3` and `4` respectively, we directly store the their values as strings and set `is_verilog_var` flag to 0.

Notice that we have duplicated information in the table, i.e. two sets of `dut.a` and `dut.b`. Compiler developers are free to optimize the information and reduce the number of information stored.
