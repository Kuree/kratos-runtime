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


if __name__ == "__main__":
    test_ncsim_continue()
