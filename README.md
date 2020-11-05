# Kratos Debugger Runtime
This is the runtime library required to debug Kratos hardware designs. It
offers realtime interaction between the simulator and a debugger, which
can either be Visual Studio Code, or any debugger that talks HTTP REST
protocol.

This library also ships with a Python API which you can use in your Python
code to control the simulator. It does not implement the full-feature of
all the protocols the runtime supports, but it's a good place to start if
you are going to implement your own debugger to interface the runtime.

## How to install kratos-runtime
There are a couple ways to install kratos-runtime. You
need a C++17 compatible compiler, such as `g++-8` to compile the library.
The Python API requires Python3 to run.

### Install from PyPI
If you're using Linux, the easiest way to install is through pip. Do
```Bash
pip install kratos-runtime
```
After installing, do
```Bash
python -c "from kratos_runtime import get_lib_path; print(get_lib_path())"
```
which will tell you where the pre-built library is and you can use that to
link to your design. You can also call `get_ncsim_flag()` to get necessary
flags.

### Build from source
If you want the built-in Python tools:
```Bash
$ git clone https://github.com/Kuree/kratos-runtime
$ cd kratos-runtime
$ git submodule update --init --recursive
$ pip install .
```
If you just want the runtime library, do:

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

#### A note for macOS
Due to the restriction of macOS, all symbols have to be resolved during the linking
time, which means all the VPI calls need to have actual symbol when linked as the
final shared object. However, this might not be doable in our case since the actual VPI
implementation is offered by the vendor. Due to limited access to macOS machines, I
am not able to resolve this issue. Feel free to send a PR if there is a fix for that.

## How to use kratos-runtime
The following instruction is based on Linux and tested against Verilator and
ncsim.

## Generate Kratos-Runtime debug database for your design
When calling `verilog()` function, you can supply another argument called 
`debug_db_filename` to specify the location where kratos can output the
debug files,
```Python
verilog(your_design, debug_db_filename="debug.db", insert_debug_info=True)
```

### Using Kratos-runtime with Verilator
Once you have compiled the shared library, you can ask
`verialtor` to link your test bench with `kratos-runtime`. Before you do that,
since we need to read the internal signals, we need to inject `verilator`
specific info via a pass:
```Python
kratos.passes.insert_verilator_public(your_design.internal_generator)
```

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

An alternative is to use the built-in Python helper class.

You can use `kratos_runtime.VerilatorTester` to run your verilator design.
```Python
with VerilatorTester(*files, cwd=temp) as tester:
    tester.run()
```
Where `files` is a list of files. By default the `run()` is non-blocking, so
you can attach your debugger with the runtime.

### Using kratos-runtime with Ncsim
Ncsim is much easier to use than `verilator`. Once you have the design, simply
tell the simulator that you want to load the vpi and dpi library, such as

```
irun test_tb.sv test.sv -sv_lib libkratos-runtime.so -loadvpi libkratos-runtime.so:initialize_runtime_vpi -access +r
```

`-access +r` is necessary to allow the runtime to read out simulation variables.

You can also use `kratos_runtime.NCSimTester` to run your design.
```Python
with NCSimTester(*files) as tester:
    tester.run()
```

### What to do after launch the simulation
You can now use any debugger that's compatible with the Kratos debug protocol.
Kratos has provide an open-source version of debugger extension inside VS Code.
You can install it
[here](https://marketplace.visualstudio.com/items?itemName=keyiz.kratos-vscode)
and use it to debug your design.
