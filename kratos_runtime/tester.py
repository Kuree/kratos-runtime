from .util import get_ncsim_flag, get_lib_path
import subprocess
from abc import abstractmethod
import os
import shutil


class Tester:
    def __init__(self, *files: str, cwd=None, clean_up_run=False, collect_coverage=False):
        self.files = []
        for file in files:
            self.files.append(os.path.abspath(file))
        self.cwd = self._process_cwd(cwd)
        self.clean_up_run = clean_up_run
        self.__process = []
        self.collect_coverage = collect_coverage

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

    def _run(self, args, cwd, env, blocking):
        if blocking:
            subprocess.check_call(args, cwd=cwd, env=env)
        else:
            p = subprocess.Popen(args, cwd=cwd, env=env)
            self.__process.append(p)

    def clean_up(self):
        if self.clean_up_run and os.path.exists(self.cwd) and os.path.isdir(
                self.cwd):
            shutil.rmtree(self.cwd)

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.clean_up()
        for p in self.__process:
            p.kill()

    def __enter__(self):
        return self


class VerilatorTester(Tester):
    def __init__(self, *files: str, cwd=None, clean_up_run=False, collect_coverage=False):
        super().__init__(*files, cwd=cwd, clean_up_run=clean_up_run, collect_coverage=collect_coverage)

    def run(self, blocking=False):
        # compile it first
        lib_name = os.path.basename(get_lib_path())
        verilator = shutil.which("verilator")
        args = [verilator, "--cc", "--exe", "--vpi", lib_name]
        args += self.files
        if self.collect_coverage:
            args += ["--coverage"]
        subprocess.check_call(args, cwd=self.cwd)
        # symbolic link it first
        env = self._link_lib(os.path.join(self.cwd, "obj_dir"))

        # find the shortest file
        mk_files = []
        for file in os.listdir(os.path.join(self.cwd, "obj_dir")):
            if file.endswith(".mk"):
                mk_files.append(file)
        mk_files.sort(key=lambda x: len(x))
        assert len(mk_files) > 0, "Unable to find any makefile from Verilator"
        mk_file = mk_files[0]
        # make the file
        subprocess.check_call(["make", "-C", "obj_dir", "-f", mk_file],
                              cwd=self.cwd, env=env)
        # run the application
        name = os.path.join("obj_dir", mk_file.replace(".mk", ""))
        print("Running " + name)
        self._run([name], self.cwd, env, blocking)


class NCSimTester(Tester):
    def __init__(self, *files: str, cwd=None, clean_up_run=False, collect_coverage=False):
        super().__init__(*files, cwd=cwd, clean_up_run=clean_up_run, collect_coverage=collect_coverage)

    def run(self, blocking=False, use_runtime=True):
        env = self._link_lib(self.cwd)
        # run it
        args = ["irun"] + list(self.files)
        if self.collect_coverage:
            # add coverage flags
            # we're only interested in the block coverage now
            # maybe convert ternary into a if statement block?
            args += ["-coverage", "b", "-covoverwrite"]
        if use_runtime:
            args += get_ncsim_flag().split()
        print("Running irun")
        self._run(args, self.cwd, env, blocking)
