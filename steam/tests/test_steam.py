"""zacos9-steam: Flatpak and native Steam, their games, and launching."""
import os, shutil, subprocess, sys, tempfile, time
import dbus

PREFIX = "org.zacos9.test.CollarExtension"
SERVICE = PREFIX + ".Steam"
PATH = "/org/zacos9/CollarExtension"
INTERFACE = "org.zacos9.CollarExtension1"

failures = 0
def check(ok, what):
	global failures
	print(("ok    " if ok else "FAIL  ") + what, flush=True)
	failures += not ok

def until(done, timeout=5.0):
	end = time.time() + timeout
	while time.time() < end:
		try:
			if done(): return True
		except dbus.DBusException:
			pass
		time.sleep(0.05)
	try: return bool(done())
	except dbus.DBusException: return False

def write(path, text, executable=False):
	os.makedirs(os.path.dirname(path), exist_ok=True)
	with open(path, "w") as f:
		f.write(text)
	if executable:
		os.chmod(path, 0o755)

def manifest(steamapps, appid, name, state=4):
	write(os.path.join(steamapps, "appmanifest_%s.acf" % appid),
		'"AppState"\n{\n\t"appid"\t\t"%s"\n\t"name"\t\t"%s"\n\t"StateFlags"\t\t"%s"\n}\n'
		% (appid, name, state))

def fake(path, log, name):
	write(path, '#!/bin/sh\necho "%s $*" >> "%s"\n' % (name, log), True)

def menu(module):
	return [(str(item[0]), str(item[1])) for item in module["menu"]]

def base_env(home, bindir):
	env = dict(os.environ)
	env["HOME"] = home
	env["PATH"] = bindir
	env["FLATPAK_USER_DIR"] = ""
	env["FLATPAK_SYSTEM_DIR"] = ""
	env["ZACOS9_COLLAR_EXTENSION_PREFIX"] = PREFIX
	return env

bus = dbus.SessionBus()

def start(binary, env):
	proc = subprocess.Popen([binary], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
	def service():
		return dbus.Interface(bus.get_object(SERVICE, PATH), INTERFACE)
	if not until(lambda: len(service().GetModules()) == 1, 10.0):
		proc.terminate()
		proc.wait()
		return None, proc
	return service(), proc

def stop(proc):
	proc.terminate()
	proc.wait()
	time.sleep(0.2)

def lines(path):
	try:
		with open(path) as f:
			return [line.rstrip("\n") for line in f]
	except OSError:
		return []

def flatpak_scenario(binary):
	print("-- Flatpak Steam", flush=True)
	home = tempfile.mkdtemp(prefix="zacos9-steam-flatpak-")
	bindir = tempfile.mkdtemp(prefix="zacos9-steam-bin-")
	log = os.path.join(home, "calls")
	try:
		os.makedirs(os.path.join(home, ".local/share/flatpak/app/com.valvesoftware.Steam/current/active"))
		steamapps = os.path.join(home, ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps")
		manifest(steamapps, 579980, "Lost Eden")
		manifest(steamapps, 1255560, "Myst")
		manifest(steamapps, 1493710, "Proton Experimental")
		manifest(steamapps, 228980, "Steamworks Common Redistributables")
		more = os.path.join(home, "Games/SteamLibrary/steamapps")
		manifest(more, 3751950, "Assassin's Creed Black Flag Resynced")
		manifest(more, 1255561, "Myst II", 6)
		write(os.path.join(steamapps, "libraryfolders.vdf"),
			'"libraryfolders"\n{\n'
			'\t"0"\n\t{\n\t\t"path"\t"%s"\n\t}\n'
			'\t"1"\n\t{\n\t\t"path"\t"%s"\n\t}\n}\n'
			% (os.path.join(home, ".var/app/com.valvesoftware.Steam/.local/share/Steam"),
				os.path.join(home, "Games/SteamLibrary")))
		fake(os.path.join(bindir, "flatpak"), log, "flatpak")

		service, proc = start(binary, base_env(home, bindir))
		if not service:
			check(False, "Flatpak Steam module appears")
			return
		try:
			m = service.GetModules()[0]
			check(str(m["id"]) == "steam", "module is Steam")
			check(bool(m["active"]), "Flatpak Steam is found")
			check(str(m["description"]) == "4 games", "games from the main and extra libraries")
			entries = menu(m)
			labels = [label for (_, label) in entries]
			ids = dict(entries)
			check("Lost Eden" in labels and "Myst II" in labels and
				"Assassin's Creed Black Flag Resynced" in labels, "installed games are listed")
			check("Proton Experimental" not in labels and
				"Steamworks Common Redistributables" not in labels, "Steam's tools are not games")
			check(ids.get("open") == "Open Steam" and ids.get("launch:579980") == "Lost Eden",
				"Open Steam and the games have their actions")

			service.Activate("steam", "launch:579980")
			check(until(lambda: "flatpak run --command=/app/bin/steam com.valvesoftware.Steam "
				"steam://rungameid/579980" in lines(log)),
				"a game starts through the Flatpak")
			service.Activate("steam", "open")
			check(until(lambda: "flatpak run com.valvesoftware.Steam" in lines(log)),
				"Open Steam starts the Flatpak")
		finally:
			stop(proc)
	finally:
		shutil.rmtree(home, ignore_errors=True)
		shutil.rmtree(bindir, ignore_errors=True)

def native_scenario(binary):
	print("-- native Steam", flush=True)
	home = tempfile.mkdtemp(prefix="zacos9-steam-native-")
	bindir = tempfile.mkdtemp(prefix="zacos9-steam-bin-")
	log = os.path.join(home, "calls")
	try:
		manifest(os.path.join(home, ".local/share/Steam/steamapps"), 440, "Team Fortress 2")
		fake(os.path.join(bindir, "steam"), log, "steam")

		service, proc = start(binary, base_env(home, bindir))
		if not service:
			check(False, "native Steam module appears")
			return
		try:
			m = service.GetModules()[0]
			check(bool(m["active"]), "native Steam is found")
			check(str(m["description"]) == "1 game", "one installed game")
			check([entry for entry in menu(m) if entry[0]] ==
				[("open", "Open Steam"), ("launch:440", "Team Fortress 2")],
				"its game is listed")
			service.Activate("steam", "launch:440")
			check(until(lambda: "steam steam://rungameid/440" in lines(log)),
				"a native game starts with steam")
		finally:
			stop(proc)
	finally:
		shutil.rmtree(home, ignore_errors=True)
		shutil.rmtree(bindir, ignore_errors=True)

def missing_scenario(binary):
	print("-- no Steam", flush=True)
	home = tempfile.mkdtemp(prefix="zacos9-steam-missing-")
	bindir = tempfile.mkdtemp(prefix="zacos9-steam-bin-")
	try:
		service, proc = start(binary, base_env(home, bindir))
		if not service:
			check(False, "the module appears without Steam")
			return
		try:
			m = service.GetModules()[0]
			check(not bool(m["active"]), "Steam is not found")
			check(menu(m) == [("", "Steam is not installed"), ("install", "Install Steam")],
				"the module offers to install it")
		finally:
			stop(proc)
	finally:
		shutil.rmtree(home, ignore_errors=True)
		shutil.rmtree(bindir, ignore_errors=True)

binary = sys.argv[1]
flatpak_scenario(binary)
native_scenario(binary)
missing_scenario(binary)

print()
print("FAILURES: %d" % failures)
sys.exit(1 if failures else 0)
