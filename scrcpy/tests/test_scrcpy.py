"""zacos9-scrcpy: module appears on a private session bus."""
import os, subprocess, sys, tempfile, time
import dbus

PREFIX = "org.zacos9.test.CollarExtension"
SERVICE = PREFIX + ".Scrcpy"
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

bus = dbus.SessionBus()
os.environ["ZACOS9_COLLAR_EXTENSION_PREFIX"] = PREFIX
binary = sys.argv[1]
env = dict(os.environ)
env["ZACOS9_COLLAR_EXTENSION_PREFIX"] = PREFIX
# fake scrcpy
tmp = tempfile.mkdtemp()
fakebin = os.path.join(tmp, "fakebin")
os.makedirs(fakebin)
with open(os.path.join(fakebin, "scrcpy"), "w") as f:
    f.write("#!/bin/sh\nsleep 30\n")
os.chmod(os.path.join(fakebin, "scrcpy"), 0o755)
env["PATH"] = fakebin + ":" + env["PATH"]

proc = subprocess.Popen([binary], env=env, stdout=subprocess.DEVNULL, stderr=open("/tmp/scrcpy_test.log","w"))
print("started pid", proc.pid, flush=True)
try:
    def get_iface():
        return dbus.Interface(bus.get_object(SERVICE, PATH), INTERFACE)
    check(until(lambda: len(get_iface().GetModules()) == 1, 10.0), "module appears")
    iface = get_iface()
    m = iface.GetModules()[0]
    check(str(m["id"]) == "scrcpy", "module is Scrcpy")
finally:
    proc.terminate()
    proc.wait()

print()
print("FAILURES: %d" % failures)
sys.exit(1 if failures else 0)