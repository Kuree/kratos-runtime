from urllib import request, error
import json


class DebuggerMock:
    def __init__(self, port=8888, design=None, prefix_top=""):
        self.port = port
        self.design = design
        self.regs = []
        self.values = []
        self._get_values()
        self.prefix_top = prefix_top

    @staticmethod
    def _get_json_header():
        return {"Content-Type'": "application/json; charset=utf-8"}

    def _get_values(self):
        if self.design is not None:
            import kratos
            import _kratos
            # get all the regs
            assert isinstance(self.design,
                              (kratos.Generator, _kratos.Generator))
            if isinstance(self.design, kratos.Generator):
                design = self.design.internal_generator
            else:
                design = self.design
            self.regs = _kratos.passes.extract_register_names(design)
            self.values = _kratos.passes.extract_var_names(design)

    def _post(self, sub_url, header=None, data=None):
        r = request.Request(
            "http://localhost:{0}/{1}".format(self.port, sub_url),
            method="POST")
        return self.__get_data(r, header, data)

    def _get(self, sub_url, header=None):
        r = request.Request(
            "http://localhost:{0}/{1}".format(self.port, sub_url),
            method="GET")
        return self.__get_data(r, header)

    @staticmethod
    def __get_data(r, header, data=None):
        if header is not None:
            for k, v in header.items():
                r.add_header(k, v)
        try:
            if data is not None:
                data = data.encode("utf-8")
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
        r = self._post("breakpoint", self._get_json_header(),
                       data)
        assert r is not None, "Unable to insert breakpoint"
        return r

    def connect(self):
        import time
        connected = False
        trial = 5
        sleep = 0.5
        for _ in range(trial):
            # trying to connect
            data = {"ip": "255.255.255.255"}
            r = self._post("connect", header=self._get_json_header(),
                           data=json.dumps(data))
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
        while True:
            r = self._get("status/simulation")
            if r is None or r == "Paused":
                break
            time.sleep(1)

    def get_value(self, handle_name):
        if self.prefix_top:
            handle_name = ".".join([self.prefix_top, handle_name])
        r = self._get("value/" + handle_name)
        if r is None or not r.isnumeric():
            r = None
        else:
            r = int(r)
        return r

    def set_pause_on_clock(self, on=True):
        r = self._post("clock/" + ("on" if on else "off"))
        assert r is not None, "Unable to pause on clock edge"

    def get_all_reg_values(self, reg_only=True):
        values = {}
        vs = self.regs if reg_only else self.values
        for name in vs:
            value = self.get_value(name)
            if value is None:
                raise Exception(
                    "Unable to get value for {0}. Got {1}".format(name, value))
            values[name] = value
        return values

    def get_io_values(self):
        in_ = {}
        out_ = {}
        if self.design is not None:
            import _kratos
            port_names = self.design.internal_generator.get_port_names()
            for port_name in port_names:
                p = self.design.ports[port_name]
                handle_name = p.handle_name()
                v = self.get_value(handle_name)
                if v is None:
                    raise Exception(
                        "Unable to get value for {0}. Got {1}".format(
                            handle_name, v))
                if p.port_direction == _kratos.PortDirection.In:
                    in_[handle_name] = v
                else:
                    out_[handle_name] = v

        return in_, out_

    def record_state(self, num_wait_reset=1, reg_only=True):
        # start the simulation until it ends
        assert self.is_paused()
        self.set_pause_on_clock(True)
        for _ in range(num_wait_reset):
            self.continue_()
            self.wait_till_pause()
        states = []
        # start recording the state
        while self.is_paused():
            regs = self.get_all_reg_values(reg_only)
            if reg_only:
                in_, out_ = self.get_io_values()
                states.append((in_, regs, out_))
            else:
                states.append(regs)
            self.continue_()
            self.wait_till_pause()
        self.wait_till_finish()
        return states

    @staticmethod
    def dump_state(state, filename):
        result = []
        for in_, reg, out_ in state:
            entry = {"in": in_, "reg": reg, "out": out_}
            result.append(entry)
        import json
        with open(filename, "w+") as f:
            json.dump(result, f)
