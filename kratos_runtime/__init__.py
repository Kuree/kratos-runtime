import os
from urllib import request
import json


def get_lib_path():
    filename = "libkratos-runtime.so"
    dirname = os.path.dirname(os.path.dirname(__file__))
    lib_path = os.path.join(dirname, filename)
    return lib_path


def get_ncsim_flag():
    return "-sv_lib libkratos-runtime.so -loadvpi libkratos-runtime.so:initialize_runtime_vpi -access +r"


def get_vcs_flag():
    return "+vpi -load libkratos-runtime.so:initialize_runtime_vpi -acc+=rw"


class DebuggerMock:
    def __init__(self, port=8888):
        self.port = port

    def continue_(self):
        r = request.Request("http://localhost:{0}/continue".format(self.port),
                            method="POST")
        resp = request.urlopen(r)
        return resp

    def insert_breakpoint(self, filename, line_num):
        data = json.dumps({"filename": filename, "line_num": line_num})
        r = request.Request("http://localhost:{0}/breakpoint".format(self.port),
                            method="POST")
        r.add_header("Content-Type'", "application/json")
        resp = request.urlopen(r, data)
        return resp
