"""zacos9-gameplayer: detection, command building and the Collar module, on a
private session bus (run under dbus-run-session).

Usage: test_gameplayer.py PATH/TO/zacos9-gameplayer
"""

import os
import subprocess
import sys
import tempfile
import time

import dbus

PREFIX = "org.zacos9.test.CollarExtension"
SERVICE = PREFIX + ".GamePlayer"
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


def iface():
    return dbus.Interface(bus.get_object(SERVICE, PATH), INTERFACE)


def module():
    modules = iface().GetModules()
    return modules[0] if len(modules) == 1 else None


def menu():
    return [(str(i), str(label), int(flags)) for i, label, flags in module()["menu"]]


def item(item_id):
    return next((m for m in menu() if m[0] == item_id), None)


def activate(item_id):
    try:
        iface().Activate("gameplayer", item_id, timeout=15)
        return None
    except dbus.DBusException as e:
        return e.get_dbus_message()


def run_check(binary, path, env):
    return subprocess.run([binary, "--check", path],
                        capture_output=True, text=True, env=env)


def make_rom(root, name, data=b""):
    p = os.path.join(root, name)
    with open(p, "wb") as f:
        f.write(data)
    return p


# The Collar module needs a private bus.
bus = dbus.SessionBus()
os.environ["ZACOS9_COLLAR_EXTENSION_PREFIX"] = PREFIX

binary = sys.argv[1]
tmp = tempfile.mkdtemp()
config = os.path.join(tmp, "config")
os.makedirs(config)
env = dict(os.environ)
env["XDG_CONFIG_HOME"] = config
env["ZACOS9_COLLAR_EXTENSION_PREFIX"] = PREFIX
env["ZACOS9_GAMEPLAYER_CORES"] = os.path.join(tmp, "cores")
os.makedirs(env["ZACOS9_GAMEPLAYER_CORES"])
env["ZACOS9_GAMEPLAYER_CONTROLLERS"] = "Test Pad"
# Fake core directory with a dummy core so buildLaunch finds it.
with open(os.path.join(env["ZACOS9_GAMEPLAYER_CORES"], "nestopia_libretro.so"), "wb"):
    pass
with open(os.path.join(env["ZACOS9_GAMEPLAYER_CORES"], "snes9x_libretro.so"), "wb"):
    pass
with open(os.path.join(env["ZACOS9_GAMEPLAYER_CORES"], "genesis_plus_gx_libretro.so"), "wb"):
    pass
with open(os.path.join(env["ZACOS9_GAMEPLAYER_CORES"], "gambatte_libretro.so"), "wb"):
    pass
with open(os.path.join(env["ZACOS9_GAMEPLAYER_CORES"], "mgba_libretro.so"), "wb"):
    pass

# ---- Detection via --check -------------------------------------------------
nes = make_rom(tmp, "game.nes", b"NES\x1A" + b"\x00" * 12)
r = run_check(binary, nes, env)
check(r.returncode == 0, "NES: --check succeeds")
check("system: NES" in r.stdout, "NES: detected as NES")
check("engine: retroarch" in r.stdout, "NES: uses RetroArch")
check("nestopia_libretro.so" in r.stdout, "NES: picks the Nestopia core")

sfc = make_rom(tmp, "game.sfc")
r = run_check(binary, sfc, env)
check(r.returncode == 0, "Super NES: --check succeeds")
check("system: Super NES" in r.stdout, "Super NES: detected")
check("snes9x_libretro.so" in r.stdout, "Super NES: picks Snes9x")

md = make_rom(tmp, "sonic.md", (b"\x00" * 0x100) + b"SEGA" + (b"\x00" * (128 * 1024 - 0x104)))
r = run_check(binary, md, env)
check(r.returncode == 0, "Sega: --check succeeds")
check("system: Sega" in r.stdout and "genesis_plus_gx_libretro.so" in r.stdout,
      "Sega: detected with Genesis Plus GX")

gb = make_rom(tmp, "game.gb")
r = run_check(binary, gb, env)
check(r.returncode == 0 and "system: Game Boy" in r.stdout and "gambatte_libretro.so" in r.stdout,
      "Game Boy: detected with Gambatte")

gba = make_rom(tmp, "game.gba")
r = run_check(binary, gba, env)
check(r.returncode == 0 and "system: Game Boy Advance" in r.stdout and "mgba_libretro.so" in r.stdout,
      "Game Boy Advance: detected with mGBA")

# PlayStation: a cue sheet plus its track.
cue = make_rom(tmp, "game.cue", b'FILE "game.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
make_rom(tmp, "game.bin", b"\x00" * 1024)
r = run_check(binary, cue, env)
check("system: PlayStation" in r.stdout, "PlayStation: a cue sheet is detected")
check("mednafen" in r.stdout, "PlayStation: uses mednafen")

n64 = make_rom(tmp, "game.n64")
env["ZACOS9_MUPEN_CORELIB"] = "/usr/lib/x86_64-linux-gnu/libmupen64plus.so.2"
env["ZACOS9_MUPEN_DATADIR"] = "/usr/share/mupen64plus"
r = run_check(binary, n64, env)
check("system: Nintendo 64" in r.stdout, "Nintendo 64: detected")
check("mupen64plus" in r.stdout, "Nintendo 64: uses mupen64plus")

# A bare .bin with no cue is an ordinary document.
alone = make_rom(tmp, "alone.bin", b"\x00" * 1024)
r = run_check(binary, alone, env)
check(r.returncode == 2 and "error:" in r.stdout, "a bare .bin is not a game")

# A .md Markdown file is not a game.
mdoc = make_rom(tmp, "readme.md", b"# hello")
r = run_check(binary, mdoc, env)
check(r.returncode == 2 and "error:" in r.stdout, "a .md document is not a game")

# ---- The Collar module while a game runs -------------------------------
# A fake engine that stays alive until it is terminated, recording its args.
fakebin = os.path.join(tmp, "fakebin")
os.makedirs(fakebin)
argslog = os.path.join(tmp, "args.txt")
fake = os.path.join(fakebin, "retroarch")
with open(fake, "w") as f:
    f.write("#!/bin/sh\nprintf '%%s\\n' \"$@\" > '%s'\nsleep 30\n" % argslog)
os.chmod(fake, 0o755)
env["PATH"] = fakebin + ":" + env["PATH"]
# Override mupen/mednafen too? The fake retroarch handles the NES game.
nes2 = make_rom(tmp, "play.nes", b"NES\x1A" + b"\x00" * 12)
proc = subprocess.Popen([binary, "--play", nes2], env=env,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    check(until(lambda: module() is not None, 10.0), "the Game Player module appears in The Collar")
    m = module()
    check(m is not None and str(m["id"]) == "gameplayer", "the module is Game Player")
    check(m is not None and "Quit Game" in [str(x[1]) for x in menu()],
          "the module offers Quit Game")
    check(m is not None and any("Test Pad" in str(x[1]) for x in menu()),
          "the module reports the auto-detected controller")
    check(item("quit") is not None, "Quit Game is an action item")
    # Quitting stops the engine and the program.
    err = activate("quit")
    check(err is None, "Quit Game succeeds")
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        check(False, "Game Player exited after Quit Game")
    else:
        check(proc.returncode == 0, "Game Player exited cleanly")
    # The engine really received the cartridge.
    if os.path.exists(argslog):
        args = open(argslog).read().splitlines()
        check(any("nestopia_libretro.so" in a for a in args), "the engine received the Nestopia core")
        check(nes2 in args, "the engine received the game file")
    else:
        check(False, "the engine recorded its arguments")
finally:
    try:
        proc.kill()
    except Exception:
        pass
    proc.wait()

print()
print("FAILURES: %d" % failures)
sys.exit(1 if failures else 0)