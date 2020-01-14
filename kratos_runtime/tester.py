from .util import get_ncsim_flag, get_lib_path
import subprocess
from abc import abstractmethod
import os


class Tester:
    def __init__(self, *files: str):
        self.files = []
        for file in files:
            self.files.append(os.path.abspath(file))

    @abstractmethod
    def run(self, cwd=None, blocking=False):
        pass

    @staticmethod
    def _process_cwd(cwd):
        if cwd is None:
            cwd = "build"
        if not os.path.isfile(cwd):
            os.makedirs(cwd, exist_ok=True)
        return cwd

    @staticmethod
    def _link_lib(path):
        lib_name = os.path.basename(get_lib_path())
        dst_path = os.path.abspath(os.path.join(path,
                                                lib_name))
        if not os.path.isfile(dst_path):
            os.symlink(get_lib_path(), dst_path)
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = os.path.dirname(dst_path)
        return env

    @staticmethod
    def _run(args, cwd, env, blocking):
        if blocking:
            subprocess.check_call(args, cwd=cwd, env=env)
        else:
            subprocess.Popen(args, cwd=cwd, env=env)


class VerilatorTester(Tester):
    def __init__(self, tb_file, *files: str):
        super().__init__(*files)
        self.tb_file = os.path.abspath(tb_file)

    def run(self, cwd=None, blocking=False):
        cwd = self._process_cwd(cwd)
        # compile it first
        lib_name = os.path.basename(get_lib_path())
        args = ["verilator", "--cc", "--exe", "--vpi", lib_name]
        args += self.files + [self.tb_file]
        subprocess.check_call(args, cwd=cwd)
        # symbolic link it first
        env = self._link_lib(os.path.join(cwd, "obj_dir"))

        # find the shortest file
        mk_files = []
        for file in os.listdir(os.path.join(cwd, "obj_dir")):
            if file.endswith(".mk"):
                mk_files.append(file)
        mk_files.sort(key=lambda x: len(x))
        assert len(mk_files) > 0, "Unable to find any makefile from Verilator"
        mk_file = mk_files[0]
        # make the file
        subprocess.check_call(["make", "-C", "obj_dir", "-f", mk_file], cwd=cwd, env=env)
        # run the application
        name = os.path.join("obj_dir", mk_file.replace(".mk", ""))
        self._run([name], cwd, env, blocking)


class NCSimTester(Tester):
    def __init__(self, *files: str):
        super().__init__(*files)

    def run(self, cwd=None, blocking=False):
        cwd = self._process_cwd(cwd)
        env = self._link_lib(cwd)
        # run it
        args = ["irun"] + list(self.files)
        args += get_ncsim_flag()
        self._run(args, cwd, env, blocking)



