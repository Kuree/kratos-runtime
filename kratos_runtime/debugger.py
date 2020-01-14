from urllib import request, error
import json


class DebuggerMock:
    def __init__(self, port=8888):
        self.port = port

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


if __name__ == "__main__":
    mock = DebuggerMock()
    print(mock.is_paused())
    print(mock.get_value("mod.in"))
    mock.continue_()
