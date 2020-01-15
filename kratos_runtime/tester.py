from .util import get_ncsim_flag, get_lib_path
import subprocess
from abc import abstractmethod
import os
import shutil


class Tester:
    def __init__(self, *files: str, cwd=None):
        self.files = []
        for file in files:
            self.files.append(os.path.abspath(file))
        self.cwd = self._process_cwd(cwd)

    @abstractmethod
    def run(self, blocking=False):
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

    def clean_up(self):
        if os.path.exists(self.cwd) and os.path.isdir(self.cwd):
            shutil.rmtree(self.cwd)

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.clean_up()

    def __enter__(self):
        return self


class VerilatorTester(Tester):
    def __init__(self, tb_file, *files: str, cwd=None):
        super().__init__(*files, cwd=cwd)
        self.tb_file = os.path.abspath(tb_file)

    def run(self, blocking=False):
        # compile it first
        lib_name = os.path.basename(get_lib_path())
        args = ["verilator", "--cc", "--exe", "--vpi", lib_name]
        args += self.files + [self.tb_file]
        subprocess.check_call(args, cwd=self.cwd)
        # symbolic link it first
        env = self._link_lib(os.path.join(self.cwd, "obj_dir"))
        # also link the cwd
        self._link_lib(self.cwd)

        # find the shortest file
        mk_files = []
        for file in os.listdir(os.path.join(self.cwd, "obj_dir")):
            if file.endswith(".mk"):
                mk_files.append(file)
        mk_files.sort(key=lambda x: len(x))
        assert len(mk_files) > 0, "Unable to find any makefile from Verilator"
        mk_file = mk_files[0]
        # make the file
        subprocess.check_call(["make", "-C", "obj_dir", "-f", mk_file], cwd=self.cwd, env=env)
        # run the application
        name = os.path.join("obj_dir", mk_file.replace(".mk", ""))
        self._run([name], self.cwd, env, blocking)


class NCSimTester(Tester):
    def __init__(self, *files: str, cwd=None):
        super().__init__(*files, cwd=cwd)

    def run(self, blocking=False):
        env = self._link_lib(self.cwd)
        # run it
        args = ["irun"] + list(self.files)
        args += get_ncsim_flag().split()
        print(args)
        self._run(args, self.cwd, env, blocking)