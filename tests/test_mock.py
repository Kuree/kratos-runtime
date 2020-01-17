from kratos_runtime import DebuggerMock
from kratos_runtime import VerilatorTester, NCSimTester
import os
import shutil
import pytest
import tempfile
import _kratos
from kratos import always_ff, posedge, Generator
import kratos.passes


use_ncsim = shutil.which("irun") is not None


def get_file_path(file):
    dir_name = os.path.dirname(__file__)
    f = os.path.join(dir_name, file)
    return os.path.abspath(f)


def template_file(src_filename, dst_filename, values):
    values = [str(v) for v in values]
    with open(src_filename) as f:
        template = f.read()
        content = template.replace("${input}", "{" + ", ".join(values) + "}")
        with open(dst_filename, "w+") as ff:
            ff.write(content)


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


class CorrectDesign0(Generator):
    def __init__(self):
        super().__init__("mod", debug=True)
        a = self.input("a", 4)
        b = self.output("b", 4)
        rst = self.reset("rst")
        clk = self.clock("clk")

        @always_ff((posedge, "clk"), (posedge, "rst"))
        def code():
            if rst:
                b = 0
            else:
                b = a + b

        self.add_always(code)


class WrongDesign0(Generator):
    def __init__(self):
        super().__init__("mod", debug=True)
        a = self.input("a", 4)
        b = self.output("b", 4)
        rst = self.reset("rst")
        clk = self.clock("clk")

        @always_ff((posedge, "clk"), (posedge, "rst"))
        def code():
            if rst:
                b = 0
            else:
                if a > 2:
                    b = a + b + 1
                else:
                    b = a + b

        self.add_always(code)


def test_state_dump():

    mod = CorrectDesign0()
    # insert verilator directives
    kratos.passes.insert_verilator_public(mod.internal_generator)
    filename = get_file_path("test_state_dump/mod.sv")
    kratos.verilog(mod, filename=filename, insert_debug_info=True,
                   insert_break_on_edge=True)
    tb_file = get_file_path("test_state_dump/test.cc.in")
    with tempfile.TemporaryDirectory() as temp:
        tb = os.path.join(temp, "test.cc")
        template_file(tb_file, tb, values=[1, 2, 3, 4])

        with VerilatorTester(tb, filename, cwd=temp) as tester:
            tester.run()
            # verilator uses TOP
            debugger = DebuggerMock(design=mod, prefix_top="TOP")
            debugger.connect()
            assert debugger.is_paused()
            # records the states
            states = debugger.record_state()
            assert len(states) == 4
            s = 0
            for i in range(4):
                s += i
                in_, regs, out_ = states[i]
                assert len(regs) == 1
                assert "mod.b" in regs
                assert regs
                assert out_["mod.b"] == s
                assert in_["mod.a"] == i + 1


def test_fault_design_coverage():
    from kratos import always_ff, Generator, posedge
    import kratos.passes

    mod = WrongDesign0()
    # insert verilator directives
    kratos.passes.insert_verilator_public(mod.internal_generator)
    filename = get_file_path("test_state_dump/mod.sv")
    kratos.verilog(mod, filename=filename, insert_debug_info=True,
                   insert_break_on_edge=True)
    tb_file = get_file_path("test_state_dump/test.cc.in")
    input_correct = [1, 2, 2, 1]
    input_wrong = [1, 2, 3, 4]
    with tempfile.TemporaryDirectory() as temp:
        with open(tb_file) as f:
            correct_tb_file = os.path.join(temp, "correct.cc")
            wrong_tb_file = os.path.join(temp, "wrong.cc")
            template_file(tb_file, correct_tb_file, input_correct)
            template_file(tb_file, wrong_tb_file, input_wrong)
        tb_files = [correct_tb_file, wrong_tb_file]
        run_states = []
        for tb_file in tb_files:
            with VerilatorTester(tb_file, filename, cwd=temp) as tester:
                tester.run()
                # verilator uses TOP
                debugger = DebuggerMock(design=mod, prefix_top="TOP")
                debugger.connect()
                assert debugger.is_paused()
                # records the states
                states = debugger.record_state(reg_only=False)
                assert len(states) == 4
                run_states.append(states)

        assert len(run_states) == 2

        # coverage based fault localization
        from _kratos import FaultAnalyzer, SimulationRun
        correct_run = SimulationRun(mod.internal_generator)
        for state in run_states[0]:
            correct_run.add_simulation_state(state)
        wrong_run = SimulationRun(mod.internal_generator)
        for state in run_states[1]:
            wrong_run.add_simulation_state(state)
        wrong_run.mark_wrong_value("mod.b")
        fault = FaultAnalyzer(mod.internal_generator)
        fault.add_simulation_run(correct_run)
        fault.add_simulation_run(wrong_run)
        stmts = fault.compute_fault_stmts_from_coverage()
        # there is only one wrong assignment here
        assert len(stmts) == 1
        # print out the faulty logic
        _kratos.util.print_stmts(stmts)


if __name__ == "__main__":
    test_fault_design_coverage()
