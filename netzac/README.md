# Netzac Navigator

A Navigator 4.0-style web browser for ZacOS 9: ZacOS 9's Platinum look,
tabbed browsing, bookmarks, Calm Mode and an adblocker. It is an ordinary
application, not a Collar extra, so its package installs a desktop entry and
nothing starts it in the background. Software adds a **Browse the Web** alias
to the Desktop when it installs the extra.

Upstream: <https://github.com/guimaraeslucas/netzac>, GPL-3.0-or-later
(see `LICENSE`). This directory is a source import of upstream `v1.0.0.1`.
To update it, replace `shell/`, `share/` and `po/` from the newer upstream
tag, keep this repository's `meson.build` hooks, and publish a new
`netzac-vVERSION` release with `scripts/release.sh`.

Unlike the rest of this repository, the import uses Qt's meta-object
compiler and a few synchronous `QProcess::waitForFinished()` calls in its
default-browser helpers, because that is how upstream is written. Review
those before importing a newer version.

The translations are compiled and installed here with the `netzac` install
tag (`po/meson.build`), rather than `i18n.gettext()`'s fixed `i18n` tag, so
`scripts/build-deb.sh netzac` puts them in the package.
