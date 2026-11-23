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
    env["HOME"] = tmp
    os.makedirs(os.path.join(tmp, "frontdesk"))
    user = pwd.getpwuid(os.getuid()).pw_name
    with open(os.path.join(tmp, "frontdesk", user), "w") as f:
        f.write("menus=restricted\nsave=user\nfinder=off\nspeech=off\ncapture=off\n"
                "zac-menu=on\nscreens=each\n")

    # No folder list: the user's Home is the one folder page.
    out = subprocess.run([exe, "--check"], env=env, capture_output=True, text=True)
    assert out.returncode == 0, out.stderr
    assert "Front Desk:" in out.stdout, out.stdout
    assert "frontDesk=1" in out.stdout, out.stdout
    assert "zacMenu=1" in out.stdout, out.stdout
    assert "screens=each" in out.stdout, out.stdout
    assert "folders=1" in out.stdout and f"first={tmp}" in out.stdout, out.stdout

    # A user's own list gives one page per folder.
    os.makedirs(os.path.join(tmp, "other"))
    folders = os.path.join(tmp, "folders")
    with open(folders, "w") as f:
        f.write(os.path.join(tmp, "other") + "\n" + tmp + "\n")
    env["ZACOS9_FRONTDESK_FOLDERS"] = folders
    out = subprocess.run([exe, "--check"], env=env, capture_output=True, text=True)
    assert out.returncode == 0, out.stderr
    assert "folders=2" in out.stdout, out.stdout
    assert f"first={tmp}/other" in out.stdout, out.stdout
print("ok: Front Desk reads its settings, its Home and its folder pages")
