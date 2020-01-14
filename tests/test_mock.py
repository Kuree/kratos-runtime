from kratos_runtime import DebuggerMock
from kratos_runtime import VerilatorTester
import os


def get_file_path(file):
    dir_name = os.path.dirname(__file__)
    f = os.path.join(dir_name, file)
    return os.path.abspath(f)


def test_continue():
    file = get_file_path("verilator/test.sv")
    tb_file = get_file_path("verilator/test.cc")
    tester = VerilatorTester(tb_file, file)
    tester.run()
    debugger = DebuggerMock()
    debugger.connect()
    assert debugger.is_paused()
    debugger.continue_()
    debugger.wait_till_finish()


if __name__ == "__main__":
    test_continue()
