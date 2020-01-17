import os
import platform
import enum


class OSType(enum.Enum):
    Linux = enum.auto()
    OSX = enum.auto()
    Windows = enum.auto()


def get_os_type():
    t = platform.system()
    if t == "Linux":
        return OSType.Linux
    elif t == "Darwin":
        return OSType.OSX
    else:
        return OSType.Windows


def get_lib_path():
    filename = "libkratos-runtime."
    os_type = get_os_type()
    if os_type == OSType.Linux:
        ext = "so"
    elif os_type == OSType.OSX:
        ext = "dylib"
    else:
        raise NotImplemented("Windows not implemented")
    filename = filename + ext
    dirname = os.path.dirname(os.path.dirname(__file__))
    lib_path = os.path.join(dirname, filename)
    assert os.path.isfile(lib_path)
    return lib_path


def get_ncsim_flag():
    return "-sv_lib libkratos-runtime.so -loadvpi libkratos-runtime.so:initialize_runtime_vpi -access +r"


def get_vcs_flag():
    return "+vpi -load libkratos-runtime.so:initialize_runtime_vpi -acc+=rw"
