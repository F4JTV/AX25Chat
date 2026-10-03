# Changelog

All notable changes to AX25Chat. `bump_version.sh` updates CMakeLists.txt,
this file and the Debian changelog together; the AppStream metadata takes
its version from CMake at build time.

## 1.6.6 - 2026-10-02

- The map and the stations lists showed at most the 60 stations heard most recently, a limit inherited from the Python version's panel that APRS-IS traffic exceeds within minutes; every station the registry holds is shown now, the expiry in Settings being the only bound, and the hub and the desktop panel say how many there are.

## 1.6.5 - 2026-10-02

- Desktop: the notification sound was QApplication::beep(), the X11 bell on Linux and MessageBeep on Windows, which most desktops keep silent. A short chime is now compiled into the program (or the WAV file named in Settings), played through QtMultimedia when the build has it and otherwise through the platform's own player (PlaySound on Windows; pw-play, paplay or aplay on Linux; afplay on macOS), the beep being the last resort only.

## 1.6.4 - 2026-10-02

- Mobile: room under the header on the preference pages; the APRS-IS position upload keeps the schedule of Location Settings (periodic interval or SmartBeaconing) instead of an interval of its own; with the device as location source the latitude and longitude fields show its live fix; swiping the application away from the recent tasks now quits it for good - the background service kept the process alive and a relaunch hung on the splash screen, since a Qt application cannot be entered twice in one process.

## 1.6.3 - 2026-10-01

- Android: the modem and the APRS-IS connection still died with the phone locked. Cause found by comparing with APRSdroid: Qt for Android suspends the application's event loop off screen unless the manifest sets android.app.background_running to true, so every timer, socket and queued signal on the Qt thread stopped at the lock. The generated manifest sets it; the foreground service falls back on the specialUse type as APRSdroid does; the APRS-IS socket carries TCP keepalive.

## 1.6.2 - 2026-10-01

- Mobile: the preference sections, the sub-sections and the stations could not be opened by tapping since 1.6.1: a card (Pane) consumes the press itself, so the tap handler placed on the item around it never saw it. A mouse area above the card's content takes the tap now, and a UI test clicks the first card through the real pointer path and checks its section opens.

## 1.6.1 - 2026-10-01

- Mobile: Material design with relief - a raised header, cards with drop shadows for the status strip, the input row, the stations and the preference sections, a round send button, accent bars on the chat's traffic; shadows had been switched off everywhere for the headless screenshots, they are now off only under the software renderer.

## 1.6.0 - 2026-10-01

- Mobile: the menu and the preferences follow APRSdroid's organisation - Show Map, Show Hub, the one-shot actions, the log, Preferences, About, Quit; and APRS Settings, APRS Connection (with the modem's and the Internet connection's own pages), Position Reports (with Location Settings and Position privacy), Chat, Display and Notifications (with Notifications and Map).
- The station can be the modem alone, the APRS Internet System alone, or both: APRS Connection on the phone, a tick box in the Modem group on the desktop; Start brings up what is chosen.

## 1.5.0 - 2026-10-01

- Desktop: a Map tab with the stations heard on the air or through APRS-IS and our own station, the same tile servers and disk cache as the phone (MapWidget); the map's settings in Configuration -> Interface.
- Desktop: the Configuration tab lays its sections one under the other, as the Python version did.
- Android: the alert sound is the phone's own default notification sound.

## 1.4.9 - 2026-10-01

- Mobile: Keep the screen on (Settings -> Interface, on by default) prevents the inactivity lock while the application is in front; the background service and the audio recovery cover a lock by hand or a switch to another application.

## 1.4.8 - 2026-10-01

- Android: when the phone locked, the audio input stream failed and Dire Wolf's receiver stopped for good at that first end of stream (Terminating after audio device 0 input failure). The Oboe backend now reopens a failed input or output stream in place, feeding silence to the demodulator meanwhile, and should the modem still fault while the station is on, the session starts it again on a growing delay (5 s to a minute).

## 1.4.7 - 2026-10-01

- Settings -> APRS position: one Reporting selector (Off, every so many minutes, SmartBeaconing) in place of two switches; SmartBeaconing is greyed out while the position comes from fixed coordinates, and falls back to the periodic report if the source is switched to fixed.
- The stations lists are alphabetical, and a station heard for the first time is inserted where it sorts (an expired one removed) without rebuilding the list, so the view no longer jumps to the top when a station is received (both interfaces).

## 1.4.6 - 2026-09-28

- Transmit: the second of two frames queued together could be handed to the modem before the transmitter had reported keying for the first, and Dire Wolf then sent it in the same keying with a few flags in place of the TXDELAY. A handed-off frame now holds the queue until the keying is reported (and unkeyed, then the inter-frame gap), so every frame gets its full TXDELAY; a modem that never reports its PTT releases the queue after five seconds.

## 1.4.5 - 2026-09-28

- Android: the modem could not find tocalls.yaml and symbols-new.txt (no folder next to the program on a phone); both are compiled in and written to the data folder at start-up.
- Start and Stop act on the station as a whole, the modem and the APRS-IS connection, on both interfaces; the connection no longer opens on its own before Start.
- Map: CARTO's tiles ask for a key and are gone; IGN Plan and orthophotos (France) and Esri World Topo and World Imagery, none needing a key, take their place.
- scripts/fetch_direwolf.sh replaces a Dire Wolf folder that holds no sources (the archive's README) instead of leaving git to refuse the clone.

## 1.4.4 - 2026-09-28

- Every installer carries the application's version in its name: AX25Chat-<version>-setup.exe (Windows), ax25chat_<version>_<arch>.deb (Linux), and now AX25Chat-<version>-<abi>.apk (Android).

## 1.4.3 - 2026-09-28

- Mobile map: tiles were resampled nearest-neighbour and drawn one tile pixel per two or three screen pixels on a phone; they are now interpolated, plain tiles are fetched one zoom level deeper and drawn at half size on high-density screens (Sharp plain tiles, on by default), and CARTO Voyager and CARTO Dark, drawn at twice the resolution, join the tile providers.

## 1.4.2 - 2026-09-28

- SmartBeaconing drives the APRS-IS position upload as well as the report on the air, and runs with the modem stopped (an Internet-only tracker); the fixed network interval steps aside while it is on.
- SmartBeaconing compared with APRSdroid's: the turn threshold uses the reference formula (turn angle + slope x 10 / speed in mph, the Kenwood units) instead of km/h, which made turns easier to trigger than intended; setting off after a report at a standstill is reported after the turn time, not the slow interval; and the speed and heading a fix leaves out are derived from the movement since the previous fix.

## 1.4.1 - 2026-09-28

- Android: the APRS-IS connection still dropped with the phone locked. The background service now holds a high-performance Wi-Fi lock (Wi-Fi sleeps with the screen), the application asks once for the Doze battery-optimisation exemption (Settings -> Interface -> Request exemption later), and it reports the service's state and the exemption in the chat after start-up.

## 1.4.0 - 2026-09-28

- APRS-IS: your own position can be uploaded periodically and right after the login (Settings -> APRS Internet System), the same report as on the air with the TCPIP* path.
- Android: a foreground service keeps the modem, the APRS-IS connection and the position working while the application is off screen (Settings -> Interface -> Keep running in the background), with a wake lock for the demodulator.

## 1.3.3 - 2026-09-28

- APRS-IS: the login used no passcode when the upload was off and then reported the passcode as refused; the passcode is now sent whenever there is one (the upload switch only decides what is sent) and the three outcomes - accepted, none given, refused - are told apart.
- Mobile: the chat follows new lines only while it is scrolled to the end; scrolled up to read, it stays put and a button offers the way back with the count of what arrived.
- The stations list is updated in place every second instead of rebuilt, so it no longer jumps while being read (both interfaces).

## 1.3.2 - 2026-09-28

- APRS-IS: stations whose callsign is not an AX.25 address (a DMR hotspot's F4ABC-D, a two-digit SSID, IR1ZWA-S) were dropped, and the decoder's complaints about them were shown as modem errors; such addresses are now replaced by a placeholder for the decoder only, the real one kept, and the decoder's output never reaches the chat.

## 1.3.1 - 2026-09-28

- Android: the map showed no tiles because the APK carried no TLS library and every https request failed; OpenSSL is now bundled (KDAB's prebuilt libraries), a failed tile is reported in the system log, and a build without TLS says so in the chat.
- Android: when the position is set to come from the device and location is switched off, the application says so and offers the system's location settings.

## 1.3.0 - 2026-09-27

- Mobile: a map (menu) of the stations heard on the air or through APRS-IS with their symbols and our own station, drawn from free tile servers (OpenStreetMap, OpenTopoMap, or a template of your own); tiles are kept on disk and served from there first, so an area seen once works offline.

## 1.2.2 - 2026-09-27

- Mobile: the settings are split into one page per section, listed with a summary line each; edits are kept until saved from any section, and leaving with unsaved changes asks.

## 1.2.1 - 2026-09-27

- APRS-IS: the passcode is computed from the callsign (Compute / Fill from callsign), upload and download are labelled as such and switching the download off resets the server's filter at once.

## 1.2.0 - 2026-09-27

- APRS Internet System: what is heard on the air is sent to the network as a receive-only IGate does (qAO, Dire Wolf's gating rules), and the traffic within a chosen radius (1 to 500 km) around the station comes back into the stations list and, on request, the chat. Regional Tier 2 servers or a host of your own, passcode, on both interfaces.

## 1.1.7 - 2026-09-27

- Mobile: no symbol artwork on the phone since 1.1.6: with Qt 6.11, QString::arg() on the symbol's char16_t picked the QChar overload and put the raw characters back into the image URL. The ids are built from explicit integers, and the provider logs an id it cannot parse.

## 1.1.6 - 2026-09-27

- Mobile: symbols whose character is special in a URL ('/', '\', '%', '#', '?') showed the wrong artwork; the image ids are now character codes.
- Mobile: a full symbol picker with both APRS tables, search, and the overlay character for the alternate table.

## 1.1.5 - 2026-09-27

- Mobile: the chat is the main screen; Stations and Settings open from the menu as pages with a back arrow, and the phone's back gesture returns to the chat.

## 1.1.4 - 2026-09-27

- Mobile: a Show modem messages switch in Settings -> Modem, off by default on a fresh installation, hides the modem's chatter from the chat.
- A flood of the same modem error line is folded into one entry and a repeat count.

## 1.1.3 - 2026-09-27

- Core: a modem restart no longer leaks the HDLC receive buffers (hdlc_rec_init allocated nine per subchannel at every start and Dire Wolf, made to start once, never freed them: after a dozen starts its leak detector flooded the log with MEMORY LEAK, rrbb_new lines). Patched to reuse them; candidate packets are freed on re-init too.
- Android: adaptive launcher icon and splash screen; the manifest is generated from Qt's own template with the splash meta-data added.
- Android: the aprs.fi symbol sheets are compiled into the APK, so the stations list and the symbol picker show the artwork; the picker lists the symbols with their icons.
- Mobile: Copy log in the menu puts the conversation on the clipboard.

## 1.1.2 - 2026-09-27

- Mobile: four themes (light, dark, red for the night, amber), chosen in Settings.
- Android: the alert sound is a ToneGenerator beep, so Test sound and the alerts work without a sound file; system notifications for traffic received while the application is in the background, with the Android 13 permission asked at start-up.

## 1.1.1 - 2026-09-27

- Mobile: full screen on Android, as RemoteRig and FT891Remote (system bars hidden from C++ and the window declared full screen).
- Mobile: the menu button is drawn instead of relying on a glyph the phone's fonts lack.
- Mobile: the location permission is requested after the microphone one instead of at the same time, when Android dropped it; the location source is reopened once granted.
- Callsign, destination, path and locator fields force upper case while typing, on both interfaces.

## 1.1.0 - 2026-09-27

- Qt Quick interface for phones and tablets (ax25chat-mobile), also usable on the desktop; the protocol logic moved into a Session shared by both interfaces.
- Android: Oboe audio backend for the Dire Wolf core, PTT through a USB sound card GPIO or a USB serial adapter (RTS/DTR) via the Android USB host API, position from the device's location service, build_android.sh.
- GPS source abstraction: serial NMEA receiver on the desktop, the device location on Android; Qt Serial Port and Qt Positioning are optional at build time.
- The settings folder is ~/.config/AX25Chat again, as in the Python version; 1.0.x wrote to a nested AX25Chat/AX25Chat folder.

## 1.0.2 - 2026-09-26

- Linux: `make_deb.sh` builds a Debian package (Ubuntu 24.04 and later,
  Debian 13 and derivatives) with the desktop entry, icons, manual page,
  AppStream metadata and the CM108 udev rule; `build_linux.sh` compiles
  and runs the tests for development.
- The data files and the symbol artwork are found from the installed
  location (`/usr/share/ax25chat`) as well as from the program's own folder.

## 1.0.1 - 2026-09-25

- Windows: `build_all.bat` builds the program with Visual Studio (clang-cl
  for the Dire Wolf core, cl for the rest), deploys the Qt runtime and the
  data files, and builds the Inno Setup installer.
- The program runs from its own folder when the data files are there, so
  the decoders find `tocalls.yaml` and `symbols-new.txt`.

## 1.0.0 - 2026-09-25

- First release of the C++/Qt 6 version, with Dire Wolf compiled in.
