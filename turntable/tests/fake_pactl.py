"""A stand-in pactl for zacos9-collar-turntable's tests.

The sound server is a JSON file, $FAKE_PACTL_STATE, which the test edits
with edit(). Beside it, STATE.events holds what `pactl subscribe` prints
(every run adds a client coming and going, as real pactl runs do) and
STATE.log one JSON list of arguments per run. "server" is 0 while the
server is down; a restart gives it a new number, which ends subscribe.

The server is this iMac's: built-in analog and HDMI (a Sony TV) outputs, the
built-in microphone, and an Audio-Technica turntable's PCM2900 USB codec,
with the loopback someone started by hand.
"""

import contextlib
import fcntl
import json
import os
import sys
import time

TURNTABLE = "alsa_input.usb-Burr-Brown_from_TI_USB_Audio_CODEC-00.analog-stereo"
CODEC_OUT = "alsa_output.usb-Burr-Brown_from_TI_USB_Audio_CODEC-00.analog-stereo"
WEBCAM = "alsa_input.usb-046d_HD_Webcam_C615_ABC123-02.analog-stereo"
MIC = "alsa_input.pci-0000_00_1b.0.analog-stereo"
HDMI = "alsa_output.pci-0000_00_03.0.hdmi-stereo"
ANALOG = "alsa_output.pci-0000_00_1b.0.analog-stereo"
HAND_LOOPBACK = 536870916


def source(index, name, description, bus, **props):
    p = {"device.bus": bus, "device.class": "sound", "media.class": "Audio/Source"}
    p.update(props)
    return {"index": index, "name": name, "description": description, "properties": p}


def monitor(index, sink, description, bus):
    return {"index": index, "name": sink + ".monitor", "description": "Monitor of " + description,
            "properties": {"device.bus": bus, "device.class": "monitor", "media.class": "Audio/Sink"}}


def turntable_sources():
    return [monitor(9128, CODEC_OUT, "PCM2900 Audio Codec Analog Stereo", "usb"),
            source(9129, TURNTABLE, "PCM2900 Audio Codec Analog Stereo", "usb",
                   **{"device.vendor.id": "0x08bb", "device.product.id": "0x2900"})]


def webcam_source():
    return source(9300, WEBCAM, "HD Webcam C615 Analog Stereo", "usb",
                  **{"device.vendor.id": "0x046d", "device.product.id": "0x082c"})


def sink(index, name, description, profile, alsa_name):
    return {"index": index, "name": name, "description": description,
            "properties": {"device.profile.name": profile, "alsa.name": alsa_name}}


def initial_state():
    return {
        "server": 1,
        "default_sink": HDMI,
        "sources": [monitor(59, ANALOG, "Built-in Audio Analog Stereo", "pci"),
                    source(60, MIC, "Built-in Audio Analog Stereo", "pci"),
                    monitor(5555, HDMI, "Built-in Audio Digital Stereo (HDMI)", "pci")] + turntable_sources(),
        "sinks": [sink(59, ANALOG, "Built-in Audio Analog Stereo", "analog-stereo", "CS4206 Analog"),
                  sink(5555, HDMI, "Built-in Audio Digital Stereo (HDMI)", "hdmi-stereo", "SONY TV"),
                  sink(9128, CODEC_OUT, "PCM2900 Audio Codec Analog Stereo", "analog-stereo", "USB Audio")],
        "modules": [{"index": 536870913, "name": "module-always-sink", "argument": ""},
                    {"index": HAND_LOOPBACK, "name": "module-loopback",
                     "argument": "source=%s sink=%s latency_msec=100" % (TURNTABLE, HDMI)}],
        "next_index": HAND_LOOPBACK + 1,
        "next_client": 20000,
        "fail_load": "",
    }


def paths(state):
    return state + ".events", state + ".log"


def create(state):
    with open(state, "w") as f:
        json.dump(initial_state(), f)
    for p in paths(state):
        open(p, "w").close()


@contextlib.contextmanager
def edit(state):
    """The server's state, to change; written back atomically, under a lock."""
    with open(state + ".lock", "a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        with open(state) as f:
            s = json.load(f)
        yield s
        with open(state + ".tmp", "w") as f:
            json.dump(s, f)
        os.replace(state + ".tmp", state)


def read(state):
    with open(state) as f:
        return json.load(f)


def event(state, *lines):
    with open(paths(state)[0], "a") as f:
        f.write("".join(line + "\n" for line in lines))


def log(state):
    with open(paths(state)[1]) as f:
        return [json.loads(line) for line in f if line.strip()]


def refuse(why="Connection refused"):
    sys.stderr.write("Connection failure: %s\n" % why)
    sys.exit(1)


def subscribe(state):
    server = read(state)["server"]
    if not server:
        refuse()
    with open(paths(state)[0]) as events:
        events.seek(0, os.SEEK_END)
        while True:
            line = events.readline()
            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
                continue
            if read(state)["server"] != server:
                refuse("Connection terminated")
            time.sleep(0.02)


def main(args):
    state = os.environ["FAKE_PACTL_STATE"]
    with open(paths(state)[1], "a") as f:
        f.write(json.dumps(args) + "\n")
    if args == ["subscribe"]:
        subscribe(state)
    with edit(state) as s:
        if not s["server"]:
            refuse()
        client = s["next_client"]
        s["next_client"] += 1
        event(state, "Event 'new' on client #%d" % client)
        out, error = run(state, s, args)
        event(state, "Event 'remove' on client #%d" % client)
    if error:
        sys.stderr.write(error + "\n")
        sys.exit(1)
    sys.stdout.write(out)


def run(state, s, args):
    """(stdout, error) for one pactl command."""
    if args == ["-f", "json", "info"]:
        return json.dumps({"server_name": "PulseAudio (on PipeWire 1.4.2)", "default_sink_name": s["default_sink"],
                           "default_source_name": MIC}), None
    if args == ["-f", "json", "list", "sources"]:
        return json.dumps(s["sources"]), None
    if args == ["-f", "json", "list", "sinks"]:
        return json.dumps(s["sinks"]), None
    if args == ["list", "short", "modules"]:
        return "".join("%d\t%s\t%s\t\n" % (m["index"], m["name"], m["argument"]) for m in s["modules"]), None
    if args[:2] == ["load-module", "module-loopback"]:
        options = dict(a.split("=", 1) for a in args[2:])
        if s["fail_load"]:
            return "", s["fail_load"]
        if options.get("source") not in [x["name"] for x in s["sources"]]:
            return "", "Failure: Module initialization failed"
        index = s["next_index"]
        s["next_index"] += 1
        s["modules"].append({"index": index, "name": "module-loopback", "argument": " ".join(args[2:])})
        event(state, "Event 'new' on module #%d" % index, "Event 'new' on sink-input #%d" % index,
              "Event 'new' on source-output #%d" % index)
        return "%d\n" % index, None
    if args[:1] == ["unload-module"] and len(args) == 2:
        index = int(args[1])
        if index not in [m["index"] for m in s["modules"]]:
            return "", "Failure: No such entity"
        s["modules"] = [m for m in s["modules"] if m["index"] != index]
        event(state, "Event 'remove' on module #%d" % index)
        return "", None
    return "", "fake pactl: no such command: %s" % " ".join(args)


if __name__ == "__main__":
    main(sys.argv[1:])
