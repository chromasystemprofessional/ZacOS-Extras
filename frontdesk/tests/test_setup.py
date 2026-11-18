#!/usr/bin/env python3
import os
import subprocess
import sys

exe = sys.argv[1]
env = dict(os.environ)
env["QT_QPA_PLATFORM"] = "offscreen"
out = subprocess.run([exe, "--check"], env=env, capture_output=True, text=True)
assert out.returncode == 0, out.stderr
assert "Front Desk Setup: accounts=" in out.stdout, out.stdout
print("ok: Front Desk Setup reads the accounts")
