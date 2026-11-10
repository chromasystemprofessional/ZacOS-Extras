"""zacos9-gameplayer: detection, command building and the Collar module, on a
private session bus (run under dbus-run-session).

Usage: test_gameplayer.py PATH/TO/zacos9-gameplayer
"""

import hashlib
import os
import struct
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


def custom_icon_bytes(path, data_home):
    """The Finder-compatible custom icon: an xattr, or the hashed PNG the
    Sniffer keeps where a filesystem has no user.* xattrs."""
    try:
        return os.getxattr(path, "user.zacos9.custom-icon")
    except OSError:
        pass
    key = "file:" + os.path.realpath(path)
    digest = hashlib.sha256(key.encode()).hexdigest()
    store = os.path.join(data_home, "zacos9", "custom-icons", digest + ".png")
    if os.path.exists(store):
        with open(store, "rb") as f:
            return f.read()
    return None


# The Collar module needs a private bus.
bus = dbus.SessionBus()
os.environ["ZACOS9_COLLAR_EXTENSION_PREFIX"] = PREFIX

binary = sys.argv[1]
tmp = tempfile.mkdtemp()
home = os.path.join(tmp, "home")
os.makedirs(home)
games = os.path.join(home, "Games")
config = os.path.join(tmp, "config")
os.makedirs(config)
env = dict(os.environ)
env["HOME"] = home
env["XDG_CONFIG_HOME"] = config
env["XDG_DATA_HOME"] = os.path.join(home, ".local", "share")
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

# Starting Game Player makes the Games folder and gives it its icon.
check(os.path.isdir(games), "Game Player creates ~/Games")
icon = custom_icon_bytes(games, env["XDG_DATA_HOME"])
check(icon is not None and icon.startswith(b"\x89PNG\r\n\x1a\n"),
      "~/Games gets a custom folder icon")
if icon:
    width, height = struct.unpack(">II", icon[16:24])
    check((width, height) == (32, 32), "the folder icon is a 32x32 PNG")

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
env["ZACOS9_GAMEPLAYER_MEDNAFEN_ID"] = "0x0003046dc21601100006000c00000000"
env["ZACOS9_GAMEPLAYER_MEDNAFEN_HAT"] = "4"
env["ZACOS9_GAMEPLAYER_CONTROLLER_MAPPINGS"] = (
    "guid,Logitech Logitech Dual Action,a:b0,b:b1,x:b2,y:b3,back:b8,start:b9,"
    "leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,righttrigger:b7,"
    "dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,")
r = run_check(binary, cue, env)
check("system: PlayStation" in r.stdout, "PlayStation: a cue sheet is detected")
check("mednafen" in r.stdout, "PlayStation: uses mednafen")
check("bios: needed" in r.stdout, "PlayStation: needs a BIOS")
check("mednafen: psx.input.port1 gamepad" in r.stdout,
      "PlayStation: configures the controller")
check("psx.input.port1.gamepad.up joystick 0x0003046dc21601100006000c00000000 "
      "abs_5- || joystick 0x0003046dc21601100006000c00000000 abs_1-" in r.stdout,
      "PlayStation: maps the D-pad and left stick")
check("psx.input.port1.gamepad.cross joystick "
      "0x0003046dc21601100006000c00000000 button_0" in r.stdout,
      "PlayStation: maps the face buttons")

# PlayStation 2: an ISO disc image.
ps2bin = os.path.join(tmp, "ps2bin")
os.makedirs(ps2bin)
with open(os.path.join(ps2bin, "PCSX2"), "w") as f:
    f.write("#!/bin/sh\n")
os.chmod(os.path.join(ps2bin, "PCSX2"), 0o755)
env["PATH"] = ps2bin + ":" + env["PATH"]
iso = make_rom(tmp, "game.iso", b"\x00" * 1024)
r = run_check(binary, iso, env)
check(r.returncode == 0, "PlayStation 2: --check succeeds")
check("system: PlayStation 2" in r.stdout, "PlayStation 2: an ISO is detected")
check(any(line.startswith("engine:") and ("PCSX2" in line or "pcsx2" in line)
          for line in r.stdout.splitlines()),
      "PlayStation 2: uses PCSX2")
check("env: GDK_BACKEND=x11" in r.stdout, "PlayStation 2: forces an X11 window")
check("bios: needed" in r.stdout, "PlayStation 2: needs a BIOS")

# PlayStation 3: a game folder, not a file.
ps3 = os.path.join(tmp, "PS3 Game")
os.makedirs(os.path.join(ps3, "PS3_GAME", "USRDIR"))
for path in (os.path.join(ps3, "PS3_GAME", "PARAM.SFO"),
             os.path.join(ps3, "PS3_GAME", "USRDIR", "EBOOT.BIN")):
    with open(path, "wb"):
        pass
rpcs3 = os.path.join(tmp, "rpcs3")
with open(rpcs3, "w") as f:
    f.write("#!/bin/sh\n")
os.chmod(rpcs3, 0o755)
env["ZACOS9_RPCS3"] = rpcs3
r = run_check(binary, ps3, env)
check(r.returncode == 0, "PlayStation 3: --check succeeds")
check("system: PlayStation 3" in r.stdout, "PlayStation 3: a game folder is detected")
check("engine: " + rpcs3 in r.stdout, "PlayStation 3: uses RPCS3")
check("bios: needed" in r.stdout, "PlayStation 3: needs its firmware")
firmware = os.path.join(config, "rpcs3", "dev_flash", "sys", "external")
os.makedirs(firmware)
with open(os.path.join(firmware, "libfs.sprx"), "wb"):
    pass
r = run_check(binary, ps3, env)
check("bios: ok" in r.stdout, "PlayStation 3: finds installed firmware")
psn = os.path.join(tmp, "PSN Game")
os.makedirs(os.path.join(psn, "USRDIR"))
for path in (os.path.join(psn, "PARAM.SFO"), os.path.join(psn, "USRDIR", "EBOOT.BIN")):
    with open(path, "wb"):
        pass
r = run_check(binary, psn, env)
check("system: PlayStation 3" in r.stdout, "an installed PS3 game is detected")
plain = os.path.join(tmp, "Plain Folder")
os.makedirs(plain)
r = run_check(binary, plain, env)
check(r.returncode == 2 and "error:" in r.stdout, "a plain folder is not a game")

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
pidlog = os.path.join(tmp, "engine.pid")
fake = os.path.join(fakebin, "retroarch")
with open(fake, "w") as f:
    f.write("#!/bin/sh\necho $$ > '%s'\nprintf '%%s\\n' \"$@\" > '%s'\nsleep 30\n"
            % (pidlog, argslog))
os.chmod(fake, 0o755)
# A fake sound server: the game's stream belongs to the fake engine's PID,
# and set-sink-input-volume records what Game Player asked for.
pactllog = os.path.join(tmp, "pactl.txt")
pactl = os.path.join(fakebin, "pactl")
with open(pactl, "w") as f:
    f.write("#!/bin/sh\n"
            "case \"$1\" in\n"
            "list)\n"
            "  echo 'Sink Input #99'\n"
            "  echo '    application.process.id = \"'$(cat '%s')'\"'\n"
            "  ;;\n"
            "set-sink-input-volume)\n"
            "  echo \"$@\" >> '%s'\n"
            "  ;;\n"
            "esac\n" % (pidlog, pactllog))
os.chmod(pactl, 0o755)
env["PATH"] = fakebin + ":" + env["PATH"]
# Override mupen/mednafen too? The fake retroarch handles the NES game.
# A PlayStation BIOS dropped in Games, a PlayStation 2 BIOS, and an
# unrelated .bin, for the firmware test while the NES game runs.
make_rom(games, "SCPH5501.BIN", b"bios")
make_rom(games, "ps2bios.bin", b"\x00" * (4 * 1024 * 1024))
make_rom(games, "track.bin", b"not firmware")
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
    check(item("volume-up") is not None and item("volume-down") is not None,
          "the module offers volume controls")
    err = activate("volume-up")
    check(err is None, "Volume Up succeeds")
    check(until(lambda: os.path.exists(pactllog) and
                "set-sink-input-volume 99 110%" in open(pactllog).read(), 5.0),
          "the game's audio stream volume is raised")
    with open(os.path.join(config, "zacos9", "gameplayer", "gameplayer.conf")) as f:
        check("volume=110" in f.read(), "the chosen volume is saved")
    medcfg = os.path.join(config, "zacos9", "gameplayer", "mednafen", "mednafen.cfg")
    check(os.path.exists(medcfg) and "sound.volume 100" in open(medcfg).read(),
          "mednafen's sound volume is 100%, not 1%")
    firmware = os.path.join(config, "zacos9", "gameplayer", "mednafen", "firmware")
    bios = os.path.join(firmware, "scph5501.bin")
    check(os.path.exists(bios) and open(bios, "rb").read() == b"bios",
          "a PlayStation BIOS .bin dropped in Games is installed for mednafen")
    check(not os.path.exists(os.path.join(games, "SCPH5501.BIN")),
          "the dropped BIOS is moved out of ~/Games")
    check(not os.path.exists(os.path.join(firmware, "track.bin")),
          "an ordinary .bin in Games is not treated as firmware")
    for pcsx2 in (os.path.join(home, ".config", "PCSX2", "bios"),
                  os.path.join(home, ".local", "share", "PCSX2", "bios")):
        check(os.path.exists(os.path.join(pcsx2, "ps2bios.bin")),
              "a PS2 BIOS .bin dropped in Games is installed for PCSX2")
    check(not os.path.exists(os.path.join(games, "ps2bios.bin")),
          "the dropped PS2 BIOS is moved out of ~/Games")
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
    medcfg = os.path.join(config, "zacos9", "gameplayer", "mednafen", "mednafen.cfg")
    medtext = open(medcfg).read() if os.path.exists(medcfg) else ""
    check("psx.input.port1.gamepad.cross" in medtext,
          "the generated mednafen config has the controller mappings")
finally:
    try:
        proc.kill()
    except Exception:
        pass
    proc.wait()

# An icon the user already chose is left alone.
try:
    os.setxattr(games, "user.zacos9.custom-icon", b"kept")
except OSError:
    pass
else:
    run_check(binary, sfc, env)
    check(os.getxattr(games, "user.zacos9.custom-icon") == b"kept",
          "an existing ~/Games icon is kept")

print()
print("FAILURES: %d" % failures)
sys.exit(1 if failures else 0)