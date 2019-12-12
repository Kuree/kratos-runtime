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
        try:
            resp = request.urlopen(r)
            return resp
        except:
            assert False, "Unable to continue"

    def insert_breakpoint(self, filename, line_num):
        data = json.dumps({"filename": filename, "line_num": line_num})
        r = request.Request(
            "http://localhost:{0}/breakpoint".format(self.port),
            method="POST")
        r.add_header("Content-Type'", "application/json")
        try:
            resp = request.urlopen(r, data)
            return resp
        except:
            assert False, "Unable to insert breakpoint"

    def connect(self):
        import time
        connected = False
        trial = 5
        sleep = 1
        for _ in range(trial):
            r = request.Request(
                "http://localhost:{0}/status".format(self.port),
                method="GET")
            try:
                request.urlopen(r)
                connected = True
                break
            except:
                time.sleep(sleep)
                sleep *= 2
        assert connected, "Unable to connect"

    def wait_till_finish(self):
        import time
        while True:
            r = request.Request(
                "http://localhost:{0}/status".format(self.port),
                method="GET")
            try:
                request.urlopen(r)
                time.sleep(1)
            except:
                return
