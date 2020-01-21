import os


def get_lib_path():
    filename = "libkratos-runtime.so"
    dirname = os.path.dirname(os.path.dirname(__file__))
    lib_path = os.path.join(dirname, filename)
    assert os.path.isfile(lib_path)
    return lib_path


def get_ncsim_flag():
    return "-sv_lib libkratos-runtime.so -loadvpi libkratos-runtime.so:initialize_runtime_vpi -access +r"


def get_vcs_flag():
    return "+vpi -load libkratos-runtime.so:initialize_runtime_vpi -acc+=rw"


def dump_icc_report(cov_directory, dst_filename, cwd=None):
    import shutil
    import tempfile
    import subprocess
    imc = shutil.which("imc")
    assert imc, "Unable to find imc from PATH"
    # dump the commands
    command = []
    # load the coverage
    command.append("load " + cov_directory)
    # dump the report
    command.append("report -detail -out {0} -source on  -covered".format(dst_filename))
    # exit
    command.append("exit")
    if cwd is None:
        cwd = "."
    filename = os.path.abspath(os.path.join(cwd, "dump_cov.cmd"))
    with open(filename, "w+") as f:
        command_str = "\n".join(command)
        f.write(command_str)
    # call imc to dump the result
    subprocess.check_call([imc, "-exec", filename], cwd=cwd)
