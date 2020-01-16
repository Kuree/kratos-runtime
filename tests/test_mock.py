from kratos_runtime import DebuggerMock
from kratos_runtime import VerilatorTester, NCSimTester
import os
import shutil
import pytest


use_ncsim = shutil.which("irun") is not None


def get_file_path(file):
    dir_name = os.path.dirname(__file__)
    f = os.path.join(dir_name, file)
    return os.path.abspath(f)


def test_verilator_continue():
    file = get_file_path("verilator/test.sv")
    tb_file = get_file_path("verilator/test.cc")
    with VerilatorTester(tb_file, file) as tester:
        tester.run()
        debugger = DebuggerMock()
        debugger.connect()
        assert debugger.is_paused()
        debugger.continue_()
        debugger.wait_till_finish()


@pytest.mark.skipif(not use_ncsim, reason="NCSim not available")
def test_ncsim_continue():
    files = [get_file_path("ncsim/test.sv"), get_file_path("ncsim/test_tb.sv")]
    with NCSimTester(*files) as tester:
        tester.run()
        debugger = DebuggerMock()
        debugger.connect()
        assert debugger.is_paused()
        debugger.continue_()
        debugger.wait_till_finish()


def test_state_dump():
    import kratos
    import _kratos
    from kratos import always_ff

    mod = kratos.Generator("mod")
    a = mod.input("a", 4)
    b = mod.output("b", 4)
    rst = mod.reset("rst")
    clk = mod.clock("clk")

    @always_ff((kratos.posedge, "clk"), (kratos.posedge, "rst"))
    def code():
        if rst:
            b = 0
        else:
            b = a + b

    mod.add_code(code)
    # insert verilator directives
    _kratos.passes.insert_verilator_public(mod.internal_generator)
    filename = get_file_path("test_state_dump/mod.sv")
    kratos.verilog(mod, filename=filename, insert_debug_info=True,
                   insert_break_on_edge=True)
    # verilator uses TOP
    tb_file = get_file_path("test_state_dump/test.cc")
    with VerilatorTester(tb_file, filename) as tester:
        tester.run()
        debugger = DebuggerMock(design=mod, prefix_top="TOP")
        debugger.connect()
        assert debugger.is_paused()
        debugger.set_pause_on_clock(True)
        debugger.continue_()
        debugger.wait_till_pause()
        s = 0
        for i in range(4):
            s += i
            regs = debugger.get_all_reg_values()
            in_, out_ = debugger.get_io_values()
            assert len(regs) == 1
            assert "mod.b" in regs
            assert regs
            assert out_["mod.b"] == s
            assert in_["mod.a"] == i + 1
            debugger.continue_()
            if i != 4 - 1:
                debugger.wait_till_pause()

        debugger.wait_till_finish()


if __name__ == "__main__":
    test_state_dump()
