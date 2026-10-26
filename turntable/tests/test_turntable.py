"""zacos9-collar-turntable against a stand-in pactl, on a private session bus (run under dbus-run-session).

Usage: test_turntable.py PATH/TO/zacos9-collar-turntable
"""

import configparser
import os
import subprocess
import sys
import tempfile
import time

import dbus

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fake_pactl  # noqa: E402
from fake_pactl import ANALOG, HAND_LOOPBACK, HDMI, TURNTABLE, WEBCAM  # noqa: E402

PREFIX = "org.zacos9.test.CollarExtension"
SERVICE = PREFIX + ".Turntable"
PATH = "/org/zacos9/CollarExtension"
INTERFACE = "org.zacos9.CollarExtension1"
ENABLED, CHECKED, SEPARATOR = 1, 2, 4

failures = 0
state = ""
config = ""


def check(ok, what):
    global failures
    print(("ok    " if ok else "FAIL  ") + what, flush=True)
    failures += not ok


def until(done, timeout=5.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            if done():
                return True
        except dbus.DBusException:
            pass
        time.sleep(0.05)
    try:
        return bool(done())
    except dbus.DBusException:
        return False


bus = dbus.SessionBus()


def iface():
    return dbus.Interface(bus.get_object(SERVICE, PATH), INTERFACE)


def module():
    modules = iface().GetModules()
    return modules[0] if len(modules) == 1 else None


def description():
    return str(module()["description"])


def menu():
    return [(str(i), str(label), int(flags)) for i, label, flags in module()["menu"]]


def item(item_id):
    return next((m for m in menu() if m[0] == item_id), None)


def activate(item_id):
    """None on success, else the error message (what The Collar would show)."""
    try:
        iface().Activate("turntable", item_id, timeout=15)
        return None
    except dbus.DBusException as e:
        return e.get_dbus_message()


def pixel(x, y):
    icon = bytes(module()["icon"])
    return tuple(icon[(y * 16 + x) * 4:(y * 16 + x) * 4 + 4])


def settings():
    s = configparser.ConfigParser()
    s.read(os.path.join(config, "zacos9", "turntable.conf"))
    return s


def calls(*prefix):
    return [c for c in fake_pactl.log(state) if c[:len(prefix)] == list(prefix)]


def loopbacks():
    return [m for m in fake_pactl.read(state)["modules"] if m["name"] == "module-loopback"]


def playing(source):
    return any(("source=" + source) in m["argument"].split() for m in loopbacks())


def unplug(*names):
    """Takes the sources away; PipeWire ends a loopback whose capture may not move."""
    with fake_pactl.edit(state) as s:
        removed = [x for x in s["sources"] if x["name"] in names]
        s["sources"] = [x for x in s["sources"] if x["name"] not in names]
        ended = [m for m in s["modules"] if m["name"] == "module-loopback" and "source_dont_move=true" in m["argument"]
                 and any(("source=" + n) in m["argument"].split() for n in names)]
        s["modules"] = [m for m in s["modules"] if m not in ended]
    fake_pactl.event(state, *(["Event 'remove' on source #%d" % x["index"] for x in removed] +
                              ["Event 'remove' on module #%d" % m["index"] for m in ended]))


def plug(*sources):
    with fake_pactl.edit(state) as s:
        s["sources"] += sources
    fake_pactl.event(state, *["Event 'new' on source #%d" % x["index"] for x in sources])


# Pixels of the record's label, the resting arm and the cartridge on the vinyl.
LABEL, REST, CARTRIDGE = (5, 8), (13, 8), (10, 10)


def main(exe):
    global state, config
    work = tempfile.TemporaryDirectory()
    config = os.path.join(work.name, "config")
    state = os.path.join(work.name, "server.json")
    fake_pactl.create(state)
    bin_dir = os.path.join(work.name, "bin")
    os.mkdir(bin_dir)
    with open(os.path.join(bin_dir, "pactl"), "w") as f:
        f.write('#!/bin/sh\nexec "%s" -I "%s" "$@"\n' % (sys.executable, fake_pactl.__file__))
    os.chmod(os.path.join(bin_dir, "pactl"), 0o755)
    env = dict(os.environ, XDG_CONFIG_HOME=config, FAKE_PACTL_STATE=state, ZACOS9_COLLAR_EXTENSION_PREFIX=PREFIX,
               PATH=bin_dir + os.pathsep + os.environ["PATH"])
    proc = subprocess.Popen([exe], env=env)
    try:
        run(proc)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait(5)
    return 1 if failures else 0


def run(proc):
    check(until(lambda: bus.name_has_owner(SERVICE)), "owns its org.zacos9.CollarExtension name")

    # Someone started a loopback from the turntable by hand: it counts as on.
    check(until(lambda: description() == "Turntable: on"), "a loopback started by hand counts as on")
    m = module()
    check(str(m["label"]) == "Turntable" and bool(m["active"]), "a single Turntable module, active")
    check(menu()[:3] == [("on", "On", ENABLED | CHECKED), ("off", "Off", ENABLED), ("", "", SEPARATOR)],
          "On is checked")
    check(("", "Input: PCM2900 Audio Codec Analog Stereo", 0) in menu(), "the input is the turntable's USB codec")
    check(("", "Output: SONY TV - Built-in Audio Digital Stereo (HDMI)", 0) in menu(),
          "the output is named as the Sound control panel names it")
    icon = bytes(m["icon"])
    check(len(icon) == 1024 and pixel(*LABEL) == (255, 0xCC, 0x33, 0x33) and pixel(*CARTRIDGE) == (255, 0xE0, 0xE0, 0xE0)
          and pixel(*REST)[0] == 0, "a 16 x 16 record with a red label and the arm on it")
    check(not os.path.exists(os.path.join(config, "zacos9", "turntable.conf")), "and no settings are written for it")

    # Off stops it, the loopback started by hand too.
    check(activate("off") is None and calls("unload-module") == [["unload-module", str(HAND_LOOPBACK)]],
          "Off unloads the loopback")
    check(until(lambda: description() == "Turntable: off"), "and the module says so")
    check(item("on") == ("on", "On", ENABLED) and item("off") == ("off", "Off", ENABLED | CHECKED), "Off is checked")
    check(pixel(*LABEL) == (255, 0xEE, 0xEE, 0xEE) and pixel(*REST) == (255, 0, 0, 0) and
          pixel(*CARTRIDGE) == (255, 0x30, 0x30, 0x30), "the label pales and the arm goes back to its rest")
    check(settings().get("turntable", "listening") == "false", "Off is remembered")

    # On plays it through the current output.
    check(activate("on") is None and until(lambda: description() == "Turntable: on"), "On plays it")
    load = calls("load-module")
    args = load[-1][2:] if load else []
    check(len(load) == 1 and "source=" + TURNTABLE in args and "latency_msec=100" in args,
          "with a loopback from the turntable, 100 ms behind")
    check("source_dont_move=true" in args, "that ends when the turntable goes, instead of moving to the microphone")
    check(not any(a.startswith("sink=") for a in args), "and follows the sound output instead of naming one")
    check("sink_input_properties=media.name=Turntable" in args, "its stream is called Turntable")
    s = settings()
    check(s.get("turntable", "listening") == "true" and s.get("turntable", "input") == TURNTABLE,
          "On and the input are remembered")

    # pactl's own runs are clients coming and going; they mustn't start more looks.
    time.sleep(0.5)
    before = len(calls("-f", "json", "list", "sources"))
    time.sleep(1.5)
    check(len(calls("-f", "json", "list", "sources")) - before == 0, "it's quiet while nothing changes")

    # The sound output changes; the loopback follows it by itself.
    with fake_pactl.edit(state) as s:
        s["default_sink"] = ANALOG
    fake_pactl.event(state, "Event 'change' on server #-1")
    check(until(lambda: ("", "Output: Built-in Audio Analog Stereo", 0) in menu()), "a new sound output is shown")
    check(len(calls("load-module")) == 1, "without starting the loopback again")

    # Unplugged: PipeWire ends the loopback.
    unplug(TURNTABLE, fake_pactl.CODEC_OUT + ".monitor")
    check(until(lambda: description() == "Turntable: not plugged in"), "unplugged, it says so")
    check(item("on") == ("on", "On", 0) and item("off") == ("off", "Off", 0), "On and Off are disabled")
    check(("", "Plug the turntable in with its USB cable.", 0) in menu(), "and it says how to plug it in")
    check(pixel(*LABEL) == (255, 0xDD, 0xDD, 0xDD) and pixel(*REST) == (255, 0x80, 0x80, 0x80), "the icon is greyed")

    # Plugged back in, it plays again: it was left on.
    plug(*fake_pactl.turntable_sources())
    check(until(lambda: description() == "Turntable: on") and playing(TURNTABLE), "plugged back in, it plays again")
    check(len(calls("load-module")) == 2, "with one new loopback")

    # The sound server restarts: its modules go, and the loopback comes back after.
    with fake_pactl.edit(state) as s:
        s["server"] = 0
        s["modules"] = []
    check(until(lambda: description() == "The sound server isn't running."), "a stopped sound server is reported")
    with fake_pactl.edit(state) as s:
        s["server"] = 2
    check(until(lambda: description() == "Turntable: on", 10) and playing(TURNTABLE),
          "when it's back, so is the turntable")
    check(len(calls("load-module")) == 3, "with one new loopback")

    # A second USB input: a webcam's microphone.
    plug(fake_pactl.webcam_source())
    check(until(lambda: item("input:" + WEBCAM) is not None), "a second USB input makes a list of inputs")
    check(item("input:" + TURNTABLE) == ("input:" + TURNTABLE, "  PCM2900 Audio Codec Analog Stereo", ENABLED | CHECKED)
          and item("input:" + WEBCAM) == ("input:" + WEBCAM, "  HD Webcam C615 Analog Stereo", ENABLED),
          "with the turntable checked")
    check(activate("input:" + WEBCAM) is None and until(lambda: playing(WEBCAM) and not playing(TURNTABLE)),
          "choosing another input while on plays that instead")
    check(len(loopbacks()) == 1 and settings().get("turntable", "input") == WEBCAM, "once, and it's remembered")
    check(activate("input:" + TURNTABLE) is None and until(lambda: playing(TURNTABLE) and not playing(WEBCAM)),
          "and back")

    # The chosen turntable unplugged never falls back to another input.
    unplug(TURNTABLE)
    check(until(lambda: description() == "Turntable: not plugged in"), "with only the webcam left, it isn't plugged in")
    check(item("on") == ("on", "On", 0) and item("input:" + WEBCAM)[2] == ENABLED, "the webcam is listed, not chosen")
    time.sleep(0.5)
    check(not playing(WEBCAM), "and isn't played in its place")

    # Left off, plugging in doesn't start it.
    check(activate("off") is None, "Off while unplugged")
    loads = len(calls("load-module"))
    plug(*fake_pactl.turntable_sources()[1:])
    check(until(lambda: description() == "Turntable: off"), "plugged in after Off, it stays off")
    time.sleep(0.5)
    check(len(calls("load-module")) == loads and not playing(TURNTABLE), "nothing is loaded")

    # A loopback that can't be made: the alert says why.
    with fake_pactl.edit(state) as s:
        s["fail_load"] = "Failure: Module initialization failed"
    error = activate("on")
    check(error is not None and "couldn't be played" in error and "Module initialization failed" in error,
          "a failed On explains itself")
    with fake_pactl.edit(state) as s:
        s["fail_load"] = ""

    # A loopback that ended before the program noticed: Off still stops it, without an alert.
    check(activate("on") is None and until(lambda: playing(TURNTABLE)), "On")
    time.sleep(0.5)
    gone = loopbacks()[0]["index"]
    with fake_pactl.edit(state) as s:
        s["modules"] = [m for m in s["modules"] if m["index"] != gone]
    check(activate("off") is None and calls("unload-module")[-1] == ["unload-module", str(gone)],
          "Off when its loopback has already ended isn't an error")
    check(until(lambda: description() == "Turntable: off"), "and it's off")

    # Quitting stops the record.
    check(activate("on") is None and until(lambda: playing(TURNTABLE)), "On again")
    proc.terminate()
    check(proc.wait(5) == 0, "SIGTERM quits it")
    check(until(lambda: not playing(TURNTABLE)), "and the record stops with it")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
