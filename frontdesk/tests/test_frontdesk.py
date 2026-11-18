#!/usr/bin/env python3
import os
import pwd
import subprocess
import sys
import tempfile

exe = sys.argv[1]
env = dict(os.environ)
env["QT_QPA_PLATFORM"] = "offscreen"
with tempfile.TemporaryDirectory() as tmp:
    env["ZACOS9_ACCOUNTS_CONF"] = tmp
    os.makedirs(os.path.join(tmp, "frontdesk"))
    user = pwd.getpwuid(os.getuid()).pw_name
    with open(os.path.join(tmp, "frontdesk", user), "w") as f:
        f.write("menus=restricted\nsave=user\nfinder=off\nspeech=off\ncapture=off\n")
    out = subprocess.run([exe, "--check"], env=env, capture_output=True, text=True)
    assert out.returncode == 0, out.stderr
    assert "Front Desk:" in out.stdout, out.stdout
    assert "frontDesk=1" in out.stdout, out.stdout
print("ok: Front Desk reads its settings and starts")
