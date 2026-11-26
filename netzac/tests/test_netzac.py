#!/usr/bin/env python3
"""Packaging checks for the Netzac extra.

The source is an upstream import; this checks the files this repository
installs and the release metadata rather than the browser's behaviour.
"""

import pathlib
import sys


def main() -> int:
    if len(sys.argv) != 5:
        print("usage: test_netzac.py DESKTOP ICON PACKAGE_CONF LINGUAS")
        return 2
    desktop, icon, package, linguas = map(pathlib.Path, sys.argv[1:5])
    failures = []

    def check(ok: bool, what: str) -> None:
        print(("ok    " if ok else "FAIL  ") + what)
        if not ok:
            failures.append(what)

    entry = desktop.read_text(encoding="utf-8")
    check("Type=Application" in entry, "the desktop entry is an application")
    check("Exec=zacos9-netzac %u" in entry, "it runs zacos9-netzac with a URL")
    check("Icon=netzac" in entry, "it names the netzac icon")
    check("Name=Netzac" in entry, "it is called Netzac")

    svg = icon.read_text(encoding="utf-8", errors="replace")
    check("<svg" in svg and "</svg>" in svg, "the icon is an SVG")

    conf = package.read_text(encoding="utf-8")
    check("package=zacos9-extra-netzac" in conf, "the package is zacos9-extra-netzac")
    check('programs="zacos9-netzac"' in conf, "removal stops zacos9-netzac")
    check("guimaraeslucas/netzac" in conf, "the package points at upstream")

    languages = [word for word in linguas.read_text(encoding="utf-8").split()
                 if word and not word.startswith("#")]
    check(bool(languages), "at least one translation is listed")
    for language in languages:
        check((linguas.parent / (language + ".po")).is_file(),
              "the " + language + " catalog exists")

    if failures:
        print(f"{len(failures)} check(s) failed")
        return 1
    print("ok    Netzac packaging")
    return 0


if __name__ == "__main__":
    sys.exit(main())
