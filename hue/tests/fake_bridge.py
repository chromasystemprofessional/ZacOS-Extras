"""A stand-in Hue Bridge speaking enough CLIP v2 for zacos9-collar-hue's tests.

Two rooms (Living Room: a colour lightstrip and a white-ambiance bulb;
Bedroom: a plain dimmable bulb), a zone holding the strip, two scenes and
the bridge_home group. Plain HTTP; the extension reaches it through
$ZACOS9_HUE_BRIDGE.
"""

import copy
import json
import queue
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

KEY = "test-application-key"


def initial_resources():
    def grouped(gid, on, bri):
        return {"id": gid, "type": "grouped_light", "on": {"on": on}, "dimming": {"brightness": bri}}

    def device(did, light):
        return {"id": did, "type": "device", "metadata": {"name": did},
                "services": [{"rid": light, "rtype": "light"}]}

    return [
        {"id": "l1", "type": "light", "metadata": {"name": "TV Lightstrip"}, "on": {"on": True},
         "dimming": {"brightness": 60.0}, "color": {"xy": {"x": 0.6915, "y": 0.3083}},
         "color_temperature": {"mirek": None, "mirek_valid": False}},
        {"id": "l2", "type": "light", "metadata": {"name": "Sofa Lamp"}, "on": {"on": True},
         "dimming": {"brightness": 60.0}, "color_temperature": {"mirek": 370, "mirek_valid": True}},
        {"id": "l3", "type": "light", "metadata": {"name": "Bedside"}, "on": {"on": False},
         "dimming": {"brightness": 40.0}},
        device("d1", "l1"), device("d2", "l2"), device("d3", "l3"),
        {"id": "r1", "type": "room", "metadata": {"name": "Living Room"},
         "children": [{"rid": "d1", "rtype": "device"}, {"rid": "d2", "rtype": "device"}],
         "services": [{"rid": "g1", "rtype": "grouped_light"}]},
        {"id": "r2", "type": "room", "metadata": {"name": "Bedroom"},
         "children": [{"rid": "d3", "rtype": "device"}],
         "services": [{"rid": "g2", "rtype": "grouped_light"}]},
        {"id": "z1", "type": "zone", "metadata": {"name": "TV Strip"},
         "children": [{"rid": "l1", "rtype": "light"}],
         "services": [{"rid": "g3", "rtype": "grouped_light"}]},
        {"id": "bh", "type": "bridge_home", "children": [],
         "services": [{"rid": "g0", "rtype": "grouped_light"}]},
        grouped("g0", True, 50.0), grouped("g1", True, 60.0), grouped("g2", False, 40.0), grouped("g3", True, 60.0),
        {"id": "s1", "type": "scene", "metadata": {"name": "Relax"}, "group": {"rid": "r1", "rtype": "room"},
         "status": {"active": "inactive"}},
        {"id": "s2", "type": "scene", "metadata": {"name": "Bright"}, "group": {"rid": "r1", "rtype": "room"},
         "status": {"active": "inactive"}},
    ]


GROUP_LIGHTS = {"g0": ["l1", "l2", "l3"], "g1": ["l1", "l2"], "g2": ["l3"], "g3": ["l1"]}


class FakeBridge:
    def __init__(self):
        self.lock = threading.Lock()
        self.resources = {r["id"]: r for r in initial_resources()}
        self.link_pressed = False
        self.puts = []
        self.streams = []
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), self._handler())
        self.server.daemon_threads = True
        self.url = "http://127.0.0.1:%d" % self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def stop(self):
        for q in list(self.streams):
            q.put(None)
        self.server.shutdown()
        self.server.server_close()

    def push(self, changes):
        """Sends an update event, as the bridge does after any change (ours or the Hue app's)."""
        with self.lock:
            for change in changes:
                merge(self.resources[change["id"]], change)
            event = [{"type": "update", "data": changes}]
            for q in self.streams:
                q.put(event)

    def apply(self, rtype, rid, body):
        changes = []
        if rtype == "grouped_light":
            lights = GROUP_LIGHTS.get(rid, [])
            part = {k: body[k] for k in ("on", "dimming") if k in body}
            changes.append(dict(id=rid, type=rtype, **part))
            changes += [dict(id=l, type="light", **part) for l in lights]
        elif rtype == "light":
            changes.append(dict(id=rid, type=rtype, **body))
        elif rtype == "scene":
            group = self.resources[rid]["group"]["rid"]
            for s in self.resources.values():
                if s["type"] == "scene" and s["group"]["rid"] == group:
                    changes.append({"id": s["id"], "type": "scene",
                                    "status": {"active": "static" if s["id"] == rid else "inactive"}})
            changes.append({"id": "g1", "type": "grouped_light", "on": {"on": True}})
        self.push(changes)

    def _handler(self):
        bridge = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def reply(self, status, body):
                data = json.dumps(body).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def body(self):
                n = int(self.headers.get("Content-Length") or 0)
                return json.loads(self.rfile.read(n) or b"{}")

            def authorised(self):
                if self.headers.get("hue-application-key") != KEY:
                    self.reply(403, {"errors": [{"description": "unauthorized user"}], "data": []})
                    return False
                return True

            def do_POST(self):
                if self.path == "/api":
                    self.body()
                    if bridge.link_pressed:
                        self.reply(200, [{"success": {"username": KEY, "clientkey": "00"}}])
                    else:
                        self.reply(200, [{"error": {"type": 101, "address": "",
                                                    "description": "link button not pressed"}}])
                else:
                    self.reply(404, {})

            def do_GET(self):
                if not self.authorised():
                    return
                if self.path == "/clip/v2/resource":
                    with bridge.lock:
                        data = copy.deepcopy(list(bridge.resources.values()))
                    self.reply(200, {"errors": [], "data": data})
                elif self.path == "/eventstream/clip/v2":
                    q = queue.Queue()
                    with bridge.lock:
                        bridge.streams.append(q)
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    self.end_headers()
                    self.wfile.write(b": hi\n\n")
                    self.wfile.flush()
                    try:
                        while True:
                            event = q.get()
                            if event is None:
                                break
                            self.wfile.write(b"id: 1:0\ndata: " + json.dumps(event).encode() + b"\n\n")
                            self.wfile.flush()
                    except OSError:
                        pass
                    finally:
                        with bridge.lock:
                            bridge.streams.remove(q)
                else:
                    self.reply(404, {})

            def do_PUT(self):
                if not self.authorised():
                    return
                parts = self.path.split("/")  # ['', 'clip', 'v2', 'resource', type, id]
                body = self.body()
                if len(parts) != 6 or parts[5] not in bridge.resources:
                    self.reply(404, {"errors": [{"description": "Not Found"}], "data": []})
                    return
                bridge.puts.append((parts[4], parts[5], body))
                self.reply(200, {"errors": [], "data": [{"rid": parts[5], "rtype": parts[4]}]})
                bridge.apply(parts[4], parts[5], body)

        return Handler


def merge(into, change):
    for k, v in change.items():
        if isinstance(v, dict) and isinstance(into.get(k), dict):
            merge(into[k], v)
        else:
            into[k] = v
