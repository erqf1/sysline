```
ERGEBNIS CLOUD-SESSION sysline

1. AUFGABE UND STATUS
Sysline 1.0.0 als neues Line-Programm bauen (Spiel-Overlay + Tasten umbelegen/Makros, Qt 6/C++ wie Clipline), mit Tests, CI, Website und Release.
Status: TEILWEISE. Programm, Tests, CI, Pakete und Website sind fertig. Das Release v1.0.0 ist NICHT erstellt: Das Pushen des Tags aus der Cloud wurde mit HTTP 403 abgelehnt. Deshalb gibt es wie vorgesehen einen PR nach main.

2. BRANCH, COMMITS, PR, CI
- Branch: claude/admiring-carson-o5law6 (ab main 96f33cf)
- Commits: f4361ab (letzter Code-Commit; davor 7af40bb, 6f4e9c7, 5aea550, ed613af, 02cf9fd), a5db56f und folgende = dieser Bericht
- PR: https://github.com/erqf1/sysline/pull/1 (Sysline 1.0.0 -> main), noch nicht gemergt
- Release-Tag: v1.0.0 lokal angelegt, Push zweimal versucht, beide Male vom Cloud-Proxy abgelehnt (HTTP 403, Richtlinie: nur der Arbeits-Branch darf gepusht werden). Den Release-Workflow per Hand zu starten ging auch nicht (404, weil release.yml noch nicht auf main liegt). Auf GitHub gibt es KEINEN Tag und KEIN Release.
- CI (ci.yml, Build + Selbsttest offscreen): grün auf Windows, Linux und macOS (Push-Läufe 6f4e9c7, 7af40bb, f4361ab). Ein PR-Lauf hatte Linux rot, nur weil die Ubuntu-Paketquelle des GitHub-Runners nicht erreichbar war (Zeitüberschreitung bei apt); nach dem Neustart grün. Damit sind alle CI-Jobs grün.
- Paket-Probelauf (release.yml beim PR): alle Jobs grün, 7 Pakete gebaut: Sysline-windows-x64-setup.exe, Sysline-windows-x64.zip, sysline_amd64.deb, sysline-linux-x86_64.tar.gz, sysline-x86_64.pkg.tar.zst, Sysline-macos-apple-silicon.dmg, Sysline-macos-intel.dmg. Der Schritt „release“ wurde übersprungen (kein Tag).

3. WAS GEBAUT WURDE
- Overlay (src/overlay.*): rahmenlos, immer vorne, durchklickbar, transparent; Ecke, Größe 70–200 %, Hintergrund-Deckkraft, Liste oder eine Zeile, Bildschirm wählbar; Ein/Aus mit Strg+Umschalt+O (src/platform_*.cpp, RegisterHotKey / X11 / Carbon); Windows: optional aus Screenshots/Streams ausgeblendet; hält sich alle 2 s wieder vorne.
- Werte (src/sensors*.cpp, src/gpulibs.*): CPU-Last, RAM, GPU-Last und VRAM (Windows: PDH „GPU Engine“ / „GPU Adapter Memory“ wie Task-Manager), GPU-Temperatur über ADL (AMD, mit Hotspot) und NVML (NVIDIA), Linux über amdgpu-sysfs/NVML; CPU-Temperatur nur unter Linux (k10temp/coretemp). Windows: bewusst weggelassen und in der App begründet (bräuchte Kernel-Treiber). Ping zu einstellbarem Host (Standard mert.win, keine IP im Code; Windows ICMP, sonst System-ping, Notlösung TCP-Verbindungsaufbau, gekennzeichnet). Uhrzeit, Akku.
- FPS (src/sensors_win.cpp): ETW-Echtzeitsitzung wie PresentMon (DXGI, D3D9, DxgKrnl Present-Ereignisse, gefiltert nach Ereignis-ID), pro Prozess im Vordergrund. Ohne Rechte zeigt das Overlay ehrlich „Adminrechte nötig“. In der App: „Als Administrator neu starten“ und „Ohne Adminrechte erlauben“ (trägt den Benutzer per UAC in die Gruppe „Leistungsprotokollbenutzer“, SID S-1-5-32-559, ein; wirkt nach Ab-/Anmelden). Linux/macOS: „nur Windows“.
- Tasten umbelegen (src/engine.*, src/platform_win.cpp): WH_KEYBOARD_LL-Hook in eigenem Thread, Taste -> Taste/Kombination oder abschalten, Senden per SendInput mit Scancodes; an/aus global und pro Profil; beim Loslassen gilt, was beim Drücken entschieden wurde (auch bei Profilwechsel).
- Makros: Schritte tippen/drücken/loslassen/warten, als Text bearbeitbar (Deutsch und Englisch), Aufnahme über den Hook, Ablauf einmal / N-mal / solange gedrückt, eigener Abspiel-Thread. Ab Werk AUS, roter Hinweis zu Spielregeln und Anti-Cheat plus Bestätigungsdialog beim Einschalten.
- Profile pro Programm: Programm im Vordergrund erkennen (Windows, X11, macOS), automatisch wechseln, Muster wie *-Win64-Shipping.exe, „Programm hinzufügen“ aus den zuletzt gesehenen Programmen, Standardprofil für alles andere.
- Oberfläche (src/mainwindow.*, src/widgets.*, src/look.*): Seiten Übersicht (Kacheln mit Verlauf), Overlay (mit Vorschau), Tasten umbelegen, Makros, Profile, Einstellungen; Tray mit Overlay/Umbelegen/Profilwahl; Autostart; Hell/Dunkel, Akzentfarbe (Standard Karmin #d61f45, kein Orange in der Auswahl), Stile Standard/Minimal/Acryl wie Clipline; Deutsch/Englisch (src/i18n_de.inc). Auf Linux/macOS sagt die UI klar, dass Umbelegen, Makros und FPS dort fehlen.
- Logo: Karmin-Kachel mit weißem Messbogen und Zeiger (gezeichnet in src/look.cpp); assets/icon.ico, icon.icns, icon.png (tools/make_icons.py).
- Updater: von Clipline übernommen (src/updater.*), Repo erqf1/sysline, Inno-AppId B7E3C1A4-5D29-4F6B-9A80-3C2E7D41F5A9; Dateinamen der Pakete passen zur Updater-Logik.
- Pakete/CI: CMakeLists.txt, packaging/windows/sysline.iss, packaging/arch/PKGBUILD, packaging/linux/install.sh, .github/workflows/ci.yml, .github/workflows/release.yml (Tag v* veröffentlicht; PR nach main baut alle Pakete zur Probe), Release-Notizen packaging/release-notes.md (kurz, Deutsch, Link auf https://mert.win/sys/changelog.html).
- Website: website/sys/index.html und website/sys/changelog.html aus website-vorlage (nur Farben --a1/--a2/--a3, data-acryl="rose" und Inhalte geändert; Links auf /app-icons/acryl.css und /app-icons/mobile.css bleiben), website/sys/icon.png, 20 echte Screenshots als WebP in website/sys/img/ (Modus --siteshots, Deutsch und Englisch). Im Changelog-Template war Orange; ersetzt durch Karmin/Rosa.

4. WAS GETESTET WURDE
- Selbsttest --selftest <ordner>, unsichtbar (QT_QPA_PLATFORM=offscreen), in der CI auf allen drei Systemen grün: Linux 105/105, macOS 105/105, Windows 104/104 (der Abspieltest für Makros läuft auf Windows absichtlich nicht, weil er echte Tasten senden würde). Geprüft: Tastennamen, Umbelegen-Logik (Reihenfolge der Modifikatoren, Wiederholung, Abschalten, links/rechts, Profilwechsel beim Halten, globales Aus), Makro-Zeitplan, Aufnahme -> Schritte, Text hin und zurück, Auslöser und „solange gedrückt“, Profilwahl (Pfad, Groß/Klein, Platzhalter, ohne .exe), Overlay-Zeilen/Größe/Platzierung in allen Ecken und auf zweitem Bildschirm, gezeichnetes Overlay, Fensterflags, Einstellungen speichern/laden inkl. Profile und Standardwerte, FPS-Zähler, GPU-Engine-Summe, Ping-Ausgabe, /proc-Parser, Tastenfelder, alle Seiten in Deutsch/Englisch und Hell/Dunkel, Neuaufbau nach Sprachwechsel, fehlende Übersetzungen (0).
- Auf dem Windows-Runner liefert PDH echte GPU-Werte (Basic Render Driver), das Programm vorne wird erkannt.
- Website lokal mit Chromium geöffnet: keine JavaScript-Fehler.
- NICHT getestet (keine Hardware in der Cloud): echte FPS in einem Spiel (ETW-Sitzung, Ereignis-IDs, Vulkan/OpenGL über DxgKrnl), echte GPU-Temperatur der RX 6600 über ADL (die ADL-Strukturen und Sensor-Nummern sind aus dem SDK übernommen, aber nie gegen einen echten Treiber gelaufen), NVML auf echter NVIDIA-Karte, Tastatur-Hook/SendInput/Makros in echten Spielen, „Ohne Adminrechte erlauben“, Neustart als Administrator, Overlay über einem echten Spiel, Installer/Updater auf einem echten Windows, macOS-DMG auf einem echten Mac. Die Werte auf den Website-Bildern sind Beispielwerte (in der Seite so gesagt).

5. OFFENE PUNKTE, RISIKEN, WAS DER NUTZER TUN MUSS
- PR #1 mergen und danach Tag v1.0.0 auf main pushen (git tag -a v1.0.0 -m "Sysline 1.0.0" && git push origin v1.0.0). Der Workflow baut dann alle Pakete und legt das Release mit den deutschen Notizen an. Danach prüfen, dass 7 Dateien im Release liegen.
- Auf dem eigenen PC (Windows, RX 6600) einmal ausprobieren: GPU-Temperatur da? FPS mit Adminrechten in einem DirectX- und einem Vulkan-Spiel? Umbelegen in einem Spiel? Falls die GPU-Temperatur fehlt: Protokoll unter %LOCALAPPDATA%\WSoftware\Sysline\sysline.log ansehen.
- Risiko: Anti-Cheat-Programme können den Tastatur-Hook oder gesendete Tasten auch ohne Makros bemerken; Umbelegen lässt sich mit einem Schalter global abschalten.
- Overlay erscheint nicht über Spielen im exklusiven Vollbild (in App und FAQ erklärt).
- Linux unter Wayland: globale Taste und automatischer Profilwechsel nur eingeschränkt (Tray-Menü geht immer).
- Website hochladen: Ordner website/sys nach mert.win/sys kopieren.

6. NÄCHSTE SCHRITTE FÜR DIE HAUPT-SESSION
1) PR https://github.com/erqf1/sysline/pull/1 mergen.
2) Tag v1.0.0 auf main pushen, Release-Workflow abwarten, 7 Pakete im Release prüfen.
3) website/sys nach Websites/sys kopieren und hochladen (img/, icon.png, index.html, changelog.html).
4) W-Hub-Katalog um Sysline ergänzen (Repo erqf1/sysline, Paketnamen wie Clipline: Sysline-windows-x64-setup.exe usw., Farbe #d61f45, Logo assets/icon.png).
5) /help-Karte für Sysline ergänzen (Icon, Karmin als --c).
6) Selbst auf Windows testen: FPS, RX-6600-Temperatur, Umbelegen, Makro-Aufnahme.
```
