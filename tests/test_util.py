from kratos_runtime.util import dump_icc_report
from kratos_runtime import NCSimTester
import shutil
import pytest
import os
import tempfile

use_ncsim = shutil.which("irun") is not None


def find_file(filename):
    dirname = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(dirname, "vectors", filename)


@pytest.mark.skipif(not use_ncsim, reason="ncsim not found in PATH")
def test_dump_icc_report():
    design = find_file("dump_icc.sv")
    tb = find_file("dump_icc_tb.sv")
    with tempfile.TemporaryDirectory() as temp:
        tester = NCSimTester(design, tb, collect_coverage=True, cwd=temp)
        tester.run(blocking=True, use_runtime=False)
        # imc always assume there is a cov_work folder in it,
        # so no need to point to that one
        reports_dir = "scope/test"
        dst_filename = "cov.txt"
        dump_icc_report(reports_dir, dst_filename, temp)
        assert os.path.isfile(os.path.join(temp, dst_filename))


if __name__ == "__main__":
    test_dump_icc_report()
