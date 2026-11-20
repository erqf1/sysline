#!/bin/sh
# Installs Sysline for the current user (~/.local). Needs Qt 6 from your distribution.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${HOME}/.local"
mkdir -p "$PREFIX/opt/sysline" "$PREFIX/bin" "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor/256x256/apps"
cp "$DIR/sysline" "$PREFIX/opt/sysline/"
ln -sf "$PREFIX/opt/sysline/sysline" "$PREFIX/bin/sysline"
cp "$DIR/icon.png" "$PREFIX/share/icons/hicolor/256x256/apps/sysline.png"
sed "s|^Exec=.*|Exec=$PREFIX/opt/sysline/sysline|" "$DIR/win.mert.Sysline.desktop" > "$PREFIX/share/applications/win.mert.Sysline.desktop"
echo "Sysline installed. Run: sysline"
