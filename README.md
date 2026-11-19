# Sysline

Spiel-Overlay (FPS, CPU-/GPU-Last, RAM, VRAM, GPU-Temperatur, Ping, Uhrzeit, Akku) plus Tasten umbelegen und Makros – Teil der W-Software-„Line“-Serie (Vidline, Clipline, Webline, Textline, Picline, Convertline).

Website: https://mert.win/sys/ · Changelog: https://mert.win/sys/changelog.html · Lizenz: Apache-2.0

## Funktionen

| | Windows | Linux | macOS |
|---|---|---|---|
| Overlay (immer vorne, durchklickbar, Ecke/Größe/Deckkraft, Strg+Umschalt+O) | ✓ | ✓ (globale Taste nur unter X11) | ✓ |
| CPU-Last, RAM | ✓ | ✓ | ✓ |
| GPU-Last, VRAM | ✓ (PDH wie Task-Manager, NVML) | ✓ (amdgpu sysfs, NVML) | GPU-Last |
| GPU-Temperatur | ✓ AMD (ADL), NVIDIA (NVML) | ✓ amdgpu, NVIDIA | – |
| CPU-Temperatur | – (bräuchte Kernel-Treiber) | ✓ (k10temp/coretemp) | – |
| Bildrate (FPS) | ✓ ETW wie PresentMon, Adminrechte oder Gruppe „Leistungsprotokollbenutzer“ | – | – |
| Ping (einstellbar, Standard mert.win) | ✓ | ✓ | ✓ |
| Tasten umbelegen, Makros | ✓ (WH_KEYBOARD_LL + SendInput) | – | – |
| Profile pro Programm (automatischer Wechsel) | ✓ | ✓ X11 | ✓ |

Makros sind ab Werk **aus**: Viele Online-Spiele verbieten sie, Anti-Cheat-Programme können sie erkennen.

## Bauen

Qt 6 und CMake:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Tests

Selbsttest ohne sichtbares Fenster (Umbelegen-Logik, Makro-Zeitplan, Profilwahl, Overlay-Layout, Einstellungen speichern/laden, alle Seiten auf Deutsch und Englisch, fehlende Übersetzungen):

```
QT_QPA_PLATFORM=offscreen build/sysline --selftest out
```

Website-Bilder: `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 build/sysline --siteshots out`, danach als WebP nach `website/sys/img/`.
Symbole: `build/sysline --icons out && python3 tools/make_icons.py out`.

`website-vorlage/` enthält die Vorlage der Line-Websites; die fertige Seite liegt in `website/sys/`.
