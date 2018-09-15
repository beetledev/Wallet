
Debian
====================
This directory contains files used to package beetlecoind/beetlecoin-qt
for Debian-based Linux systems. If you compile beetlecoind/beetlecoin-qt yourself, there are some useful files here.

## beetlecoin: URI support ##


beetlecoin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install beetlecoin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your beetlecoinqt binary to `/usr/bin`
and the `../../share/pixmaps/beetlecoin128.png` to `/usr/share/pixmaps`

beetlecoin-qt.protocol (KDE)

