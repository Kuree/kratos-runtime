from urllib import request, error
import json


class DebuggerMock:
    def __init__(self, port=8888, design=None, top_prefix="TOP"):
        self.port = port
        self.design = design
        if top_prefix is not None:
            self.top_prefix = top_prefix + "."
        else:
            self.top_prefix = ""
        self.regs = []
        self._get_regs()

    def _get_regs(self):
        if self.design is not None:
            import kratos
            import _kratos
            # get all the regs
            assert isinstance(self.design, [kratos.Generator, _kratos.Generator])
            if isinstance(self.design, kratos.Generator):
                design = self.design.internal_generator
            else:
                design = self.design
            self.regs = _kratos.passes.extract_register_names(design)

    def _post(self, sub_url, header=None, data=None):
        r = request.Request("http://localhost:{0}/{1}".format(self.port, sub_url),
                            method="POST")
        return self.__get_data(r, header, data)

    def _get(self, sub_url, header=None):
        r = request.Request("http://localhost:{0}/{1}".format(self.port, sub_url),
                            method="GET")
        return self.__get_data(r, header)

    @staticmethod
    def __get_data(r, header, data=None):
        if header is not None:
            for k, v in header.items():
                r.add_header(k, v)
        try:
            resp = request.urlopen(r, data)
            result = resp.read()
            if resp.code != 200:
                return None
            return result.decode("ascii")
        except error.URLError as ex:
            return None

    def continue_(self):
        r = self._post("continue")
        assert r is not None, "Unable to continue"
        return r

    def insert_breakpoint(self, filename, line_num):
        data = json.dumps({"filename": filename, "line_num": line_num})
        r = self._post("breakpoint", {"Content-Type'": "application/json"}, data)
        assert r is not None, "Unable to insert breakpoint"
        return r

    def connect(self):
        import time
        connected = False
        trial = 5
        sleep = 0.5
        for _ in range(trial):
            r = self._get("status")
            if r is not None:
                connected = True
                break
            time.sleep(sleep)
            sleep *= 2
        assert connected, "Unable to connect"

    def wait_till_finish(self):
        import time
        while True:
            r = self._get("status")
            if r is None:
                break
            time.sleep(1)

    def is_paused(self):
        r = self._get("status/simulation")
        return r is not None and r == "Paused"

    def wait_till_pause(self):
        import time
        while not self.is_paused():
            time.sleep(1)

    def get_value(self, handle_name):
        r = self._get("value/" + handle_name)
        return r

    def set_pause_on_clock(self, on=True):
        r = self._post("clock/" + ("on" if on else "off"))
        assert r is not None, "Unable to pause on clock edge"

    def get_all_reg_values(self):
        values = {}
        for name in self.regs:
            _name = self.top_prefix + name
            value = self.get_value(_name)
            if value is None or not name.isnumeric():
                raise Exception("Unable to get value for {0}. Got {1}".format(_name, value))
            values[name] = value
        return values
