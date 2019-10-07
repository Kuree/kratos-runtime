# Kratos Debugger Runtime
This is the runtime library required to debug Kratos hardware designs. You
need a C++17 compatible compiler, such as `g++-8` to compile the library.

## How to use Kratos-Runtime
The following instruction is based on Linux and tested against Verilator and
ncsim.

```Bash
$ git clone https://github.com/Kuree/kratos-runtime
$ cd kratos-runtime
$ git submodule update --init --recursive
$ mkdir build
$ cd ./build
$ cmake ..
$ make -j
```

After that, you can find the library as `build/src/libkratos-runtime.so`. You
can either copy that library to any place you like or simply  use symbolic
link.

### Generate Kratos-Runtime debug database for your design
When calling `verilog()` function, you can supply another argument called 
`debug_db_filename` to specify the location where kratos can output the
debug files,
```Python
verilog(design, debug_db_filename="debug.db")
```

### Using Kratos-runtime with Verilator
Once you have compiled the shared library, you can ask
`verialtor` to link your test bench with `kratos-runtime`. Before you do that,
since we need to read the internal signals, we need to inject `verilator`
specific info via a pass:
```Python
_kratos.passes.insert_verilator_public(design.internal_generator)
```
`_kratos` is the namespace for native C++ binding, you can use it via
`import _kratos`.

When invoking the `verilator` command, you need to specify the kratos runtime
name as well as `--vpi` switch, for instance:
```Bash
verilator --cc design.sv test_tb.sv libkratos-runtime.so --vpi --exe
```

You can symbolic link `libkratos-runtime.so` inside `obj_dir` so that the linker
and find it easily.

Once the test bench is compiled, you need to use `LD_LIBRARY_PATH` to let the
system to load, such as
```
$ LD_LIBRARY_PATH=./obj_dir/ ./obj_dir/Vtest
```
Or you can let the linker to fix the shared library path in the `verilator`,
which is beyond the scope of this tutorial.

### Using kratos-runtime with Ncsim
Ncsim is much easier to use than `verilator`. Once you have the design, simply
tell the simulator that you want to load the vpi and dpi library, such as

```
irun test_tb.sv test.sv -sv_lib libkratos-runtime.so -loadvpi libkratos-runtime.so:initialize_runtime_vpi -access +r
```

`-access +r` is necessary to allow the runtime to read out simulation variables.

### What to do after launch the simulation
You can now use any debugger that's compatible with the Kratos debug protocol.
Kratos has provide an open-source version of debugger extension inside VS Code.
You can install it
[here](https://marketplace.visualstudio.com/items?itemName=keyiz.kratos-vscode)
and use it to debug your design.