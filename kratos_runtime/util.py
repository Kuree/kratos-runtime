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
