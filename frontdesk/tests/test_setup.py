#!/usr/bin/env python3
import configparser
import os
import subprocess
import sys

exe = sys.argv[1]
env = dict(os.environ)
env["QT_QPA_PLATFORM"] = "offscreen"
out = subprocess.run([exe, "--check"], env=env, capture_output=True, text=True)
assert out.returncode == 0, out.stderr
assert "Front Desk Setup: accounts=" in out.stdout, out.stdout

# Front Desk Setup is the extra's control panel: it must be in Control Panels
# even when no account is running Front Desk.
desktop = os.path.join(os.path.dirname(__file__), "..", "data",
                       "zacos9-frontdesk-setup.desktop")
parser = configparser.ConfigParser(interpolation=None)
parser.optionxform = str
with open(desktop, encoding="utf-8") as f:
    parser.read_file(f)
entry = parser["Desktop Entry"]
categories = [c for c in entry.get("Categories", "").split(";") if c]
assert "X-ZacOS9-ControlPanel" in categories, categories
assert entry.get("Exec", "").startswith("zacos9-frontdesk-setup"), entry.get("Exec")
print("ok: Front Desk Setup reads the accounts and is a control panel")
