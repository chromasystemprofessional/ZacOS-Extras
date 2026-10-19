"""zacos9-collar-hue against a fake Hue Bridge, on a private session bus (run under dbus-run-session).

Usage: test_hue.py PATH/TO/zacos9-collar-hue
"""

import configparser
import os
import subprocess
import sys
import tempfile
import time

import dbus

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fake_bridge import KEY, FakeBridge  # noqa: E402

PREFIX = "org.zacos9.test.CollarExtension"
SERVICE = PREFIX + ".Hue"
PATH = "/org/zacos9/CollarExtension"
INTERFACE = "org.zacos9.CollarExtension1"
ENABLED, CHECKED, SEPARATOR = 1, 2, 4

failures = 0


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


def modules():
    return {str(m["id"]): m for m in iface().GetModules()}


def labels():
    return [str(m["label"]) for m in iface().GetModules()]


def menu(module):
    return [(str(i), str(label), int(flags)) for i, label, flags in modules()[module]["menu"]]


def item(module, item_id):
    return next((m for m in menu(module) if m[0] == item_id), None)


def activate(module, item_id):
    """None on success, else the error message (what The Collar would show)."""
    try:
        iface().Activate(module, item_id, timeout=15)
        return None
    except dbus.DBusException as e:
        return e.get_dbus_message()


def pixel(module, x, y):
    icon = bytes(modules()[module]["icon"])
    a, r, g, b = icon[(y * 16 + x) * 4:(y * 16 + x) * 4 + 4]
    return (a, r, g, b)


def main(exe):
    bridge = FakeBridge()
    config = tempfile.TemporaryDirectory()
    env = dict(os.environ, XDG_CONFIG_HOME=config.name, ZACOS9_HUE_BRIDGE=bridge.url,
               ZACOS9_COLLAR_EXTENSION_PREFIX=PREFIX)
    proc = subprocess.Popen([exe], env=env)
    try:
        run(bridge, config.name, proc)
    finally:
        proc.terminate()
        proc.wait(5)
        bridge.stop()
    return 1 if failures else 0


def run(bridge, config, proc):
    check(until(lambda: bus.name_has_owner(SERVICE)), "owns its org.zacos9.CollarExtension name")

    # Not paired yet: one Hue module that walks through the link button.
    check(until(lambda: list(modules()) == ["hue"]), "unpaired: a single Hue module")
    check(item("hue", "pair") == ("pair", "Connect to Hue Bridge", ENABLED), "it offers Connect to Hue Bridge")
    icon = bytes(modules()["hue"]["icon"])
    check(len(icon) == 1024 and pixel("hue", 7, 5) == (255, 0xDD, 0xDD, 0xDD), "a 16 x 16 unlit bulb")
    error = activate("hue", "pair")
    check(error is not None and "link button" in error, "Connect before the button is pressed says to press it")

    bridge.link_pressed = True
    check(activate("hue", "pair") is None, "Connect after the button is pressed succeeds")
    with open(os.path.join(config, "zacos9", "hue.conf")) as f:
        conf = f.read()
    check(KEY in conf and bridge.url not in conf, "the key is saved; a test bridge's address isn't")
    mode = os.stat(os.path.join(config, "zacos9", "hue.conf")).st_mode & 0o777
    check(mode == 0o600, "hue.conf is readable only by its owner")

    # Paired: just the Hue module, until rooms are chosen from it.
    check(until(lambda: labels() == ["Hue"]), "paired: only the Hue module until rooms are chosen")
    m = modules()
    check(bool(m["hue"]["active"]) and str(m["hue"]["description"]) == "Hue: 2 lights on",
          "the Hue module counts lit lights")
    hue = menu("hue")
    check(hue[0] == ("all-off", "Turn Off All Lights", ENABLED), "Turn Off All Lights starts the Hue menu")
    check([i for i in hue if i[0].startswith("show:")] ==
          [("show:r2", "  Bedroom", ENABLED), ("show:r1", "  Living Room", ENABLED),
           ("show:z1", "  TV Strip", ENABLED)], "it lists rooms and zones together by name, unchecked")

    check(activate("hue", "show:z1") is None and until(lambda: labels() == ["Hue", "TV Strip"]),
          "choosing a zone adds its module")
    check(activate("hue", "show:r1") is None and until(lambda: labels() == ["Hue", "Living Room", "TV Strip"]),
          "choosing a room adds its module, in name order")
    check(item("hue", "show:r1") == ("show:r1", "  Living Room", ENABLED | CHECKED) and
          item("hue", "show:r2") == ("show:r2", "  Bedroom", ENABLED), "and checks it")
    check(activate("hue", "show:z1") is None and until(lambda: labels() == ["Hue", "Living Room"]),
          "choosing it again removes it")
    check(activate("hue", "show:r2") is None and until(lambda: labels() == ["Hue", "Bedroom", "Living Room"]),
          "choosing another room adds it")
    settings = configparser.ConfigParser()
    settings.read(os.path.join(config, "zacos9", "hue.conf"))
    check(settings.get("strip", "rooms") == "r1, r2" and KEY in settings.get("bridge", "key"),
          "the choices are saved beside the key")
    mode = os.stat(os.path.join(config, "zacos9", "hue.conf")).st_mode & 0o777
    check(mode == 0o600, "and hue.conf stays private")
    m = modules()
    check(bool(m["r1"]["active"]) and not m["r2"]["active"], "a lit room is active, a dark one isn't")
    check(str(m["r1"]["description"]) == "Living Room: on, 60%" and str(m["r2"]["description"]) == "Bedroom: off",
          "descriptions give state and brightness")
    a, r, g, b = pixel("r1", 7, 5)
    check(a == 255 and r > g and r > b, "the lit room's bulb glows its lights' colour (red strip, warm lamp)")
    check(pixel("r2", 7, 5) == (255, 0xDD, 0xDD, 0xDD), "the dark room's bulb is unlit")

    lr = menu("r1")
    check(lr[0] == ("on", "On", ENABLED | CHECKED) and lr[1] == ("off", "Off", ENABLED),
          "On is checked in a lit room")
    check(("", "Brightness: 60%", 0) in lr and
          [i[0] for i in lr if i[0].startswith("brightness:")] == ["brightness:25", "brightness:50", "brightness:75",
                                                                    "brightness:100"] and
          not any(i[2] & CHECKED for i in lr if i[0].startswith("brightness:")),
          "four brightness levels, none checked at 60%")
    check(("scene:s2", "  Bright", ENABLED) in lr and ("scene:s1", "  Relax", ENABLED) in lr,
          "the room's scenes, by name")
    check(lr[-1] == ("scene:s1", "  Relax", ENABLED) and
          not any(i[0].startswith(("color:", "all-off")) for i in lr), "scenes end the menu: no colours or All Off")

    # Commands go to the bridge; its events bring the new state back.
    bridge.puts.clear()
    check(activate("r1", "off") is None and bridge.puts == [("grouped_light", "g1", {"on": {"on": False}})],
          "Off turns the room's grouped light off")
    check(until(lambda: str(modules()["r1"]["description"]) == "Living Room: off"),
          "the bridge's event updates the module")
    check(until(lambda: pixel("r1", 7, 5) == (255, 0xDD, 0xDD, 0xDD)), "and unlights its bulb")

    bridge.puts.clear()
    check(activate("r1", "brightness:25") is None and
          bridge.puts == [("grouped_light", "g1", {"on": {"on": True}, "dimming": {"brightness": 25}})],
          "a brightness level turns the room on at that level")
    check(until(lambda: item("r1", "brightness:25") == ("brightness:25", "  25%", ENABLED | CHECKED)),
          "and is checked")

    bridge.puts.clear()
    check(activate("r1", "scene:s1") is None and bridge.puts == [("scene", "s1", {"recall": {"action": "active"}})],
          "a scene is recalled")
    check(until(lambda: item("r1", "scene:s1")[2] == ENABLED | CHECKED), "and checked once active")

    # Something else (the Hue app, a switch) changes a light.
    bridge.push([{"id": "g2", "type": "grouped_light", "on": {"on": True}, "dimming": {"brightness": 80.0}},
                 {"id": "l3", "type": "light", "on": {"on": True}, "dimming": {"brightness": 80.0}}])
    check(until(lambda: str(modules()["r2"]["description"]) == "Bedroom: on, 80%"),
          "changes made elsewhere show up from the event stream")

    bridge.puts.clear()
    check(activate("hue", "all-off") is None and bridge.puts == [("grouped_light", "g0", {"on": {"on": False}})],
          "Turn Off All Lights uses the bridge's all-lights group")

    # The bridge goes away.
    bridge.stop()
    check(until(lambda: list(modules()) == ["hue"], 15) and
          "isn't responding" in str(modules()["hue"]["description"]), "an unreachable bridge says so")
    check(item("hue", "retry") == ("retry", "Try Again", ENABLED), "and offers Try Again")
    check(proc.poll() is None, "the extension is still running")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
