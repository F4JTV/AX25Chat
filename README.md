# AX25Chat (C++ / Qt 6)

Port of the AX25Chat packet radio chat application to C++17 and Qt 6, with
the Dire Wolf modem compiled into the executable instead of being run as a
separate program.

Status: **complete port, plus a mobile interface.** The embedded modem core, its Qt engine, the
protocol layer, the model layer and the user interface (chat page,
configuration page, symbol picker, notifications) are all in place, with
unit tests for every layer below the UI and an end-to-end test that runs the
window over the real modem on generated audio. What is not ported: the
Windows installer script and the PyInstaller build files, which described a
Python deployment. See *Roadmap* for the remaining items.

## What is here

```
CMakeLists.txt                  top-level project
cmake/DirewolfCore.cmake        builds the Dire Wolf core as a static library
cmake/toolchain-mingw-w64.cmake cross-compiles the core for Windows from Linux
build_linux.sh                  Linux: dependencies, Dire Wolf sources, build, tests
make_deb.sh                     Linux: build and package as .deb
bump_version.sh                 bump the version in CMake and both changelogs
packaging/                      desktop entry, icons, man page, AppStream, Debian files, udev rule
build_all.bat                   Windows: fetch, build, deploy, installer, in one go
installer/AX25Chat.iss          Inno Setup script (English, French)
src/core/msvc/                  unistd.h and compatibility shims for the Microsoft runtime
patches/direwolf/               one patch on top of Dire Wolf 1.8 (see below)
scripts/fetch_direwolf.sh       clones Dire Wolf 1.8 into external/ and patches it
src/core/dw_embed.h             C API of the embedded core
src/core/dw_embed.c             replaces direwolf.c: start / run / stop, frame dispatch
src/core/dw_textcolor.c         replaces textcolor.c: console output -> host callback
src/core/dw_embed_shim.h        reroutes exit() calls inside Dire Wolf to the host
src/core/dw_stubs.c             no-ops for the Dire Wolf modules left out
src/engine/DirewolfEngine.*     Qt wrapper: own thread, signals, queries
src/protocol/AX25Frame.*        AX.25 addresses and frames, fragmentation, FCS
src/protocol/AprsPacket.h       decoded APRS packet and weather report
src/protocol/AprsDecoder.*      APRS decoding over Direwolf's decode_aprs.c
src/protocol/AprsEncoder.*      position reports, coordinate formatting, Maidenhead
src/protocol/DirewolfHeaders.h  the Direwolf headers made safe to include from C++
src/model/AppConfig.*           configuration model and JSON persistence
src/model/ChannelManager.*      transmit queue and the busy/clear decision
src/model/SmartBeacon.*         speed and turn adaptive position interval
src/model/MessageManager.*      APRS messages, acknowledgements, retries, reply-ack
src/model/StationRegistry.*     stations heard, distance and bearing
src/model/PeriodicSender.*      beacon and position report timer
src/model/GpsReceiver.*         NMEA 0183 parser and serial GPS receiver
src/model/ModemConfigFile.*     where direwolf.conf lives, starter file, CHANNEL lines
src/ui/MainWindow.*             the window: chat page, toolbar, status bar, wiring
src/ui/SettingsPage.*           the Configuration tab, the sections one under the other
src/ui/MapWidget.*              the Map tab: tiles, markers, mouse (desktop)
src/model/TileProviders.*       the keyless tile servers both maps draw from
src/model/TileCache.*           map tiles kept on disk and served from there first
src/ui/SymbolPicker.*           APRS symbol chooser and the symbol name tables
src/ui/SymbolArt.*              rendering symbols from the aprs.fi sprite sheets
src/ui/Notifier.*               per-category alerts with rate limiting
src/ui/Style.*                  palette-aware colours
src/app/main.cpp                entry point and command line (desktop)
src/model/Session.*             the running station without a UI, shared by both interfaces
src/model/AprsIsClient.*        the APRS Internet System connection (login, range filter, gating)
src/model/GpsProvider.*         GPS source interface: serial NMEA or the device location
src/model/PhoneLocation.*       Qt Positioning as a GPS source (Android)
src/mobile/                     the Qt Quick interface: MobileApp controller and QML pages
src/mobile/TileCache.h          the disk-cached network manager handed to the QML engine
src/core/audio_oboe.cpp         Dire Wolf's audio.h on Oboe (Android)
src/core/ptt_android.c          Dire Wolf's ptt.h through the host's USB code (Android)
src/engine/AndroidPtt.*         JNI bridge to UsbPtt.java
android/                        UsbPtt.java and the launcher icon (Qt's manifest template is used)
build_android.sh                Android: toolchain setup, build, sign, install
scripts/fetch_symbols.sh        downloads the aprs.fi symbol sheets into assets/
scripts/run_app_test.sh         runs the end-to-end test with generated audio
src/tools/dwcore_test.cpp       console harness for the C core (no Qt)
src/tools/dwengine_demo.cpp     console harness for the Qt engine
tests/tst_protocol.cpp          unit tests, protocol layer (QtTest)
tests/tst_model.cpp             unit tests, configuration and channel access
tests/tst_aprs_encoder.cpp      unit tests, encoder and SmartBeaconing
tests/tst_messaging.cpp         unit tests, messaging, stations, periodic sender
tests/tst_gps.cpp               unit tests, NMEA parsing and line handling
tests/tst_app.cpp               end-to-end: the window over the real modem
conf/test-stdin.conf            core configuration reading raw audio from stdin
conf/test-null.conf             core configuration for a machine without a sound card
```

## Building

Dependencies:

| Platform | Required | Optional |
|---|---|---|
| Linux (Debian/Ubuntu names) | `cmake ninja-build g++ qt6-base-dev qt6-serialport-dev libasound2-dev` | `libudev-dev` (CM108 PTT), `libhamlib-dev` (PTT through rigctld) |
| Windows | MSYS2 with the MinGW-w64 toolchain and CMake, a Qt 6 MinGW kit | — |
| macOS | Xcode command line tools, CMake, Qt 6, `portaudio` (Homebrew) | `hidapi` (CM108 PTT), `hamlib` |

On Windows the Dire Wolf core needs a compiler that accepts GCC
extensions; see *Windows: build and installer* below for the Visual Studio
route (clang-cl for the C core, cl for the rest) and the MinGW alternative.

On Ubuntu 24.04 or Debian 13, the two scripts at the project root do
everything, dependencies included:

```sh
./build_linux.sh --test      # dependencies (asks), Dire Wolf sources, build, unit tests
./make_deb.sh --check        # ... and a Debian package, checked with lintian
```

By hand, the same thing is:

```sh
./scripts/fetch_direwolf.sh          # clones wb2osz/direwolf tag 1.8, applies the patch
cmake -B build -G Ninja
cmake --build build
```

This builds the `ax25chat` application, the libraries, the console tools
and the tests. `AX25CHAT_BUILD_ENGINE=OFF` builds only the C core and
`dwcore_test`, which needs no Qt at all; `AX25CHAT_BUILD_APP=OFF` leaves out
the Qt Widgets application. `ctest` in the build directory runs the unit
tests, and the end-to-end test too when Dire Wolf's `gen_packets` is on
`PATH` (see *Trying it without a radio*).

Symbol icons are optional: `scripts/fetch_symbols.sh` downloads the aprs.fi
sheets into `assets/aprs-symbols`, which the application looks for beside
its executable (`assets/aprs-symbols`), one level up, in
`share/ax25chat/aprs-symbols`, or in the application data folder. Without
them the picker and the station list show names and codes.

The Direwolf data files the decoders read (`tocalls.yaml` for the device
identification, `symbols-new.txt` for the newer overlay symbols) are copied
to `build/data/` at configure time, which is where the tools find them when
run from the build directory; the application will install them beside its
executable.

The Dire Wolf tree is not committed to this repository. The recommended
long-term arrangement is a fork of Dire Wolf with the patch committed on a
branch, added here as a git submodule at `external/direwolf`; the script
exists so the project builds without that fork.

## SmartBeaconing

The interval between position reports follows the speed, as HamHUD and
the Kenwood radios do it: the slow interval below the stopped speed, the
fast one above the fast speed, and in between inversely proportional to
the speed so the distance between reports stays about the same (the
Kenwood defaults: 8 to 96 km/h, 30 minutes to 3 minutes). Three things
pre-empt the interval: a turn wider than `turn angle + turn slope × 10 /
speed in mph` (28° and 26, the Kenwood defaults) once the turn time has
passed; setting off after a report made at a standstill, after the turn
time; and the first fix. Compared with APRSdroid's implementation, which
was read for this: the same corner pegging and the same "started moving"
rule; APRSdroid interpolates the interval linearly where this follows the
reference's inverse rule; and, as APRSdroid does, the speed a receiver
leaves out of a fix is derived from the movement since the previous one
(the larger of the two counts), as is the heading.

## APRS Internet System (aprs.fi)

Settings has an *APRS Internet System* group, on both interfaces, and the
station can be the modem alone, the Internet connection alone, or both
(*APRS Connection* on the phone, the two tick boxes on the desktop):
*Start* (the header button, Ctrl+K on the desktop) brings up what is
chosen, *Stop* takes it down. With the connection on, the program connects
to a Tier 2 server on the client-defined filter
port 14580 — the network's regional rotate address for your region
(`euro`, `noam`, `soam`, `asia`, `aunz`, or `rotate` for any), or a host
of your own — and logs in with your callsign and its passcode, which is
derived from the callsign without SSID and filled in by a button. Upload
and download are separate switches:

- **What is heard on the air goes to the network**, and so to aprs.fi:
  every packet heard that decodes as APRS is sent as a receive-only IGate
  sends it, with the `qAO` construct and your callsign appended to the
  path, and the rules of Dire Wolf's own IGate — nothing whose path says
  `TCPIP`, `TCPXX`, `NOGATE` or `RFONLY`, no third-party packets, no
  generic queries, the information part cut at the first CR or LF, and a
  digipeated copy heard again within a minute not sent twice. The chat
  traffic of this program is not APRS and is never sent. This needs the
  passcode; without it the connection is accepted but nothing you send is.
- **The traffic around you comes back**: a range filter (`r/lat/lon/km`,
  1 to 500 km around the GPS fix or the fixed coordinates, re-sent when
  the station moves) fills the stations list with what the network hears,
  tagged `[IS]`; the chat shows it too if you ask for it, since a few
  hundred kilometres of APRS can be relentless. Messages addressed to you
  are always shown; they are not acknowledged, this is a receive-only
  connection.

- **Your own position**, on request: the same report as on the air
  (symbol, comment, GPS or fixed coordinates), sent straight to the network
  with the `TCPIP*` path on the schedule of the position reports — the
  periodic interval, or SmartBeaconing following the movement — and right
  after the login; needs the passcode. It works with the modem stopped: a
  phone that is only an Internet tracker.

The connection is kept alive with a comment line every minute, watched
for silence, and reconnected with a growing delay. The status bar (desktop)
and the chat strip (mobile, `IS`) show its state.

## Linux: build and Debian package

`build_linux.sh` is the development build: it offers to install the build
dependencies with apt (`--deps` installs without asking, `--no-deps` never
looks), fetches and patches the Dire Wolf sources on the first run,
configures with Ninja, compiles, and with `--test` runs the unit tests;
`--run` starts the program, `--symbols` fetches the aprs.fi artwork,
`--debug` and `--clean` do what they say. Nothing is installed.

`make_deb.sh` compiles in `build-deb/` and packages with CPack into
`ax25chat_<version>_<arch>.deb` at the project root. Dependencies are not
written by hand: `dpkg-shlibdeps` reads the libraries actually linked into
the program, so the list is right on Ubuntu as on a Raspberry Pi. The
package installs the program, the data files under `/usr/share/ax25chat`,
a desktop entry, icons, a manual page, AppStream metadata, the changelog and
copyright, and the udev rule that lets the `audio` group reach a CM108 sound
card's PTT line. `--symbols` includes the aprs.fi symbol artwork (fetched if
missing; its `COPYRIGHT.md` is worth reading before redistributing the
package), `--check` runs lintian, `--clean` starts from scratch. Install
with `sudo apt install ./ax25chat_<version>_<arch>.deb` — apt rather than
`dpkg -i`, so the dependencies come along.

Both scripts share `packaging/build-deps.sh`, the one list of build
dependencies, which also bounds the number of parallel jobs by the memory
available.

`bump_version.sh x.y.z "change" ["change"...]` updates `CMakeLists.txt`,
`CHANGELOG.md` and the Debian changelog together; every fix and every
feature gets a new version.

## Android: the mobile interface

`ax25chat-mobile` is a Qt Quick interface over the same session, modem and
settings file as the desktop program: the chat is the screen, with the
channel state and the input row. The menu follows APRSdroid's: *Show Map*,
*Show Hub* (the stations: tap one to address it, long-press for its
details), then *Send position*, *Beacon now*, *Cancel queue*, then *Copy
log*, *Clear log*, then *Preferences*, *About*, *Quit*. The preferences
follow APRSdroid's sections too — *APRS Settings* (callsign, SSID,
operator, locator), *APRS Connection* (what Start brings up: the modem,
the APRS Internet System, or both; each with its own page; the digipeater
path), *Position Reports* (symbol, comment, Location Settings, Position
privacy), *Chat, beacon and messages* (ours), *Display and Notifications*
(screen, theme, Notifications, Map, the background service) — one page
per section with a summary line under its name; the modem page generates
the modem
configuration from a few choices — speed, PTT method, channel access — so
the operator never edits `direwolf.conf` on a phone. It also runs on the
desktop (`./build/ax25chat-mobile`), which is how it is tested.

Dire Wolf's `tocalls.yaml` and `symbols-new.txt` are compiled in and
written to the application's data folder at start-up, which becomes the
working directory, since a phone has no folder next to the program for
the modem to search.

What Android needs that a desktop has: **audio**, through Oboe
(`src/core/audio_oboe.cpp` implements Dire Wolf's `audio.h` on Oboe's
blocking streams; `ADEVICE default` or a numeric device id, the configured
`ARATE` honoured by Oboe's resampler; a stream that fails — the phone
locking, a route change, a USB card unplugged for a moment — is reopened
in place with silence fed to the demodulator meanwhile, since Dire Wolf's
receiver stops for good at the first end of stream, and should the modem
still fault while the station is on, it is started again on a growing
delay); **PTT**, through the USB host API
(`android/src/org/ax25chat/UsbPtt.java`: a CM108 sound card's GPIO by HID
output report — the same four bytes Dire Wolf writes — or the RTS/DTR line
of a CDC-ACM, FTDI, CP210x, CH34x or PL2303 serial adapter by the chip's
own control request; `ptt_android.c` replaces `ptt.c` in the core and
keeps its DLQ notifications), and **position** from the device's location
service through Qt Positioning (`PhoneLocation`), behind the same
`GpsProvider` interface as the serial NMEA receiver. Android asks the user
for USB permission per device: the first attempt asks, the answer opens the
device, the next transmission keys it. On the desktop, the alert is a short two-note chime compiled into the
program (`assets/sounds/notify.wav`), or the WAV file named in Settings:
played through QSoundEffect when the build has QtMultimedia, otherwise
through what the platform offers — `PlaySound` on Windows, `pw-play`,
`paplay` or `aplay` on Linux, `afplay` on macOS — and only as a last
resort the system beep, which most desktops keep silent (that beep was the
previous default, which is why nothing was heard). On Android, alerts go
through `org.ax25chat.Notifications`: the phone's own default notification sound
(the Qt beep does nothing on Android, and a fresh installation has no sound
file), and a system notification when traffic arrives while the application is in the
background — Android 13 and later ask the user for that permission, after
the microphone and location ones.

| Chat | Stations | Settings | Symbol picker |
|---|---|---|---|
| ![chat](docs/screenshots/mobile-chat.png) | ![stations](docs/screenshots/mobile-stations.png) | ![settings](docs/screenshots/mobile-settings.png) | ![symbols](docs/screenshots/mobile-symbols.png) |

The symbol picker lists both APRS tables with their artwork and a search,
and for the alternate table an overlay character (a digit or a capital
letter drawn over the symbol, sent as the table identifier).

**Map** (menu on the phone, a tab on the desktop): every station heard on
the air or through APRS-IS and not yet forgotten (Settings → *Forget a
station after*, two hours by default), with their symbols, and our own
station, on a
slippy map drawn from free tile
servers that need no key or account — OpenStreetMap by default,
OpenTopoMap, IGN's Plan and orthophotos (France, its open "découverte"
services), Esri's World Topo and World Imagery, or any `{z}/{x}/{y}`
template of your own (Settings → Interface). On a
high-density screen, plain 256 px tiles are stretched over two or three
physical pixels each and look soft; *Sharp plain tiles* fetches the next
zoom level and draws it at half size, crisp at the price of four times the
tiles and the map's own labels half as big. Pan by dragging,
zoom by pinching, double tap, wheel or the buttons; centre on yourself or
fit every station; tap a marker for the station's details and to address
it a message. Tiles are kept on disk under the application's data folder
(size chosen in Settings, 200 MB by default) and served from there first,
so an area seen once stays available with no connection at all; the tile
servers' terms (an identifiable User-Agent, a cache, their credit shown)
are honoured.

Four themes, chosen in Settings: light, dark, red for the night (every
colour a shade of red, so the eyes keep their dark adaptation) and amber
for dim light.

![themes](docs/screenshots/mobile-themes.png)

Material design throughout: a raised header, cards with drop shadows for
the status strip, the input row, the stations and the preference sections,
a round send button, and in the chat a card with an accent bar in the
entry's colour for traffic (sent, received, positions), the system's own
notes staying plain. The shadows are drawn by the device's GPU; the
screenshots below come from the software renderer and show the layout flat.

The screenshots are rendered by the interface itself on a desktop, at phone
size, with `AX25CHAT_SCREENSHOT_DIR=<folder> QT_QPA_PLATFORM=offscreen
QT_QUICK_BACKEND=software ./build/ax25chat-mobile --modem-conf conf/test-stdin.conf
< test.wav`: the same pages, the same Material style, fed with the test
generator's frames.

Build from Ubuntu 24.04:

```sh
./build_android.sh --setup --install
```

The launcher icon is adaptive (`android/res/mipmap-*`), the splash screen
is `android/res/drawable/splash.xml`, and the manifest is generated by the
script from Qt's own template with the splash meta-data added, so it never
lags behind the Qt installed. The aprs.fi symbol sheets are compiled into
the APK (fetched by the script; `--no-symbols` leaves them out — read their
`COPYRIGHT.md` before redistributing an APK that carries them). The menu's
*Copy log* puts the conversation on the clipboard, for a report.

The APK is written as `AX25Chat-<version>-<abi>.apk` at the project root,
next to what the other platforms produce — `AX25Chat-<version>-setup.exe`
from `build_all.bat`, `ax25chat_<version>_<arch>.deb` from `make_deb.sh` —
and carries the same version inside (`versionName`, and a `versionCode`
derived from it, 1.4.4 → 10404).

`--setup` installs the JDK, adb, Qt for Android and the host Qt
(aqtinstall in a virtual environment), adds the Positioning and Multimedia
modules to a kit that lacks them, installs the SDK platform, build tools
and NDK, and creates the signing key (keep it: an update must carry the
same key as the version it replaces). The manifest is Qt's own template,
filled from the target properties in `CMakeLists.txt` (package
`org.ax25chat`, icon, permissions declared with `qt_add_android_permission`). Then the script fetches Dire Wolf, fetches
Oboe (CMake FetchContent), compiles, packages, signs, and installs on the
phone connected by USB. Afterwards `--install`, `--reinstall`, `--logcat`,
`--no-sign`, `--clean`. Versions and paths (Qt 6.11.2, NDK 27.2.12479018,
platform 36) are variables at the top, each overridable from the
environment.

**Running off screen.** The decisive point, found by comparing with
APRSdroid: Qt for Android **suspends the application's event loop** when
its activity leaves the screen unless the manifest says
`android.app.background_running = true` — and with the template's default
of `false`, every timer, socket and queued signal on the Qt thread stops
the moment the phone locks: the APRS-IS keepalives stop, the server drops
the connection, the modem's reports queue up unprocessed. `build_android.sh`
sets it to true in the manifest it generates. First, too, the screen need
not go off at all while the application is in front: *Keep the screen on* (Settings → Interface,
on by default) sets the window flag Android provides for that, so the
inactivity lock does not happen. Beyond that, Android suspends an
application minutes after it leaves the screen, which cuts the modem's audio, the APRS-IS socket and the
GPS. `KeepAliveService.java` is a foreground service in the application's
own process: its permanent notification ("AX25Chat is running") is what
lets Android keep the process alive, and a partial wake lock keeps the CPU
up for the demodulator. Its type is chosen from the permissions granted at
the moment it starts — `microphone` and `location`, which Android 14+
require for those to keep working in the background, else `specialUse`
as APRSdroid declares it (Android 15 caps `dataSync` at six hours a day).
The APRS-IS socket also carries TCP keepalive, as APRSdroid's does. The service also holds a
high-performance Wi-Fi lock, as FT891Remote does for its audio stream:
Wi-Fi goes to sleep with the screen and a TCP connection dies with it.
Settings → Interface → *Keep running in the background* switches it off.
The service is declared in the manifest by `build_android.sh` alongside
the splash screen. Swiping the application away from the recent tasks
quits it for good: the service ends the process, because a Qt application
cannot be entered a second time in a process it is still running in — the
next launch would otherwise hang on the splash screen.

Two more things Android does when the phone is locked, which no service
overrides: **Doze** suspends the network of every application that is not
on the battery-optimisation exemption list, so the application asks once
for the exemption through the system's own dialog (Settings → Interface →
*Request exemption* later), and some manufacturers (Samsung, Xiaomi,
Huawei…) add their own "sleeping apps" lists, where AX25Chat has to be
allowed by hand. The chat reports the service's state and the exemption a
few seconds after start-up.

## Windows: build and installer

Install once: Visual Studio 2022 or 2026 with the "Desktop development
with C++" workload **plus the individual component "C++ Clang Compiler for
Windows"**, Qt 6 for MSVC 64-bit (Widgets, Network, Serial Port;
Multimedia for custom notification sounds, Positioning for the device's
location, Quick and Quick Controls 2 for the touch interface, which builds
and runs on Windows as well), git, and Inno Setup 6. Then, in an **x64
Native Tools Command Prompt**, from the project folder:

```bat
build_all.bat /deps
```

The script fetches the Dire Wolf sources (tag 1.8) and applies the patch,
downloads the symbol artwork, configures and compiles, gathers the program
with the Qt runtime, the data files, the artwork and the Visual C++
redistributable into `installer\dist`, and compiles
`installer\AX25Chat.iss` into `installer\output\AX25Chat-<version>-setup.exe`.
Edit `QT_DIR` at the top of the script if yours differs.

| Option | Effect |
|---|---|
| `/deps` | fetch and patch Dire Wolf, fetch the symbol sheets, if missing |
| `/clean` | wipe the build directory first |
| `/nobuild` | skip configure and compile; deploy and package only |
| `/noinstaller` | stop after staging `installer\dist` |
| `/test` | run the unit tests after compiling |

**The manifest and lld-link.** clang-cl links with lld-link, and Qt gives
every executable a manifest (`longPathAware`, `asInvoker`) that CMake hands
to the linker as `/MANIFESTINPUT`. lld-link merges its own trustInfo block
into it and, in the versions Visual Studio ships, namespace-prefixes the
merged attributes (`ms_asmv1:level`), which Windows rejects: the program
then refuses to start with "side-by-side configuration is incorrect" (error
14001). The CMake applies Qt's own cure, `/MANIFESTUAC:NO`, which keeps
the linker from adding its block.

**Why clang-cl.** The Dire Wolf sources use GCC extensions
(`__attribute__((hot))`, `__builtin_popcount`) and POSIX headers, which
`cl.exe` rejects. clang-cl, the Clang compiler Visual Studio ships, accepts
them and produces MSVC-ABI objects, so the C core built by clang-cl links
with the C++ built by `cl` and with the Qt MSVC binaries. `build_all.bat`
configures with `-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=cl` and
the Ninja generator; the CMake stops with a clear message if the C compiler
is `cl`. `src/core/msvc/` supplies what the Microsoft runtime lacks and
the sources expect: `unistd.h`, the `__WIN32__` macro MinGW defines,
`strcasecmp`, `localtime_r`/`gmtime_r`.

The installer is bilingual (English, French), installs the Visual C++
runtime when it is missing, and leaves the settings in `%APPDATA%\AX25Chat`
alone on uninstall.

`build_all.bat`, the `.iss` and the `.rc` must keep CRLF line endings:
cmd.exe misparses a batch file with bare line feeds. `.gitattributes` pins
them, `scripts/check_line_endings.sh` (`--fix` to repair) checks them, and
`scripts/check_batch.py` checks the batch file against two rules of
cmd.exe that leave only "X was unexpected at this time" behind: a `)` in
the text of an `echo` inside a parenthesised block closes the block, and a
`/?` as the first word after `rem` or `echo` asks for the command's help.

**MinGW alternative.** With the Qt MinGW kit and MSYS2's MinGW-w64
toolchain, nothing special is needed: `cmake -B build -G Ninja` and build.
`cmake/toolchain-mingw-w64.cmake` cross-compiles the core from Linux with
`gcc-mingw-w64-x86-64`, which is how the Windows code paths of the core
(`_beginthreadex`, `CRITICAL_SECTION`, the Windows audio and PTT modules,
hidapi, the bundled regex) are checked without a Windows machine.

## Trying it without a radio

The core reads raw audio from standard input when the configuration says
`ADEVICE stdin`, and Dire Wolf's own `gen_packets` tool produces audio from
packet text. From a stock Dire Wolf build:

```sh
printf 'F1ABC-7>CHAT,WIDE1-1:Hello from the test generator\n' > msgs.txt
gen_packets -o test.wav msgs.txt
./build/dwcore_test -c conf/test-stdin.conf -t 4 < test.wav
./build/dwengine_demo -c conf/test-stdin.conf -t 4 < test.wav
```

Both print the decoded frame, the DCD going busy and clear around it, and
the end-of-input condition surfacing as a *fault* rather than as a process
exit. On Linux, `conf/test-null.conf` uses ALSA's `null` plugin so that the
transmit path can be exercised without hardware:

```sh
./build/dwcore_test -c conf/test-null.conf -t 3 --twice -s "N0CALL>CHAT:test"
```

This queues one UI frame, shows PTT keying and the transmit queue draining,
stops the core, then starts it again to prove the stop/start path.

The same audio drives the whole application headless:

```sh
PATH=$PATH:/path/to/direwolf/build/src scripts/run_app_test.sh build
```

`tst_app` builds a `MainWindow` on `conf/test-stdin.conf`, waits for the
modem to start and decode the frames, checks the chat and monitor lines,
the station list with distance and bearing, then types a message and checks
that it is held back while the carrier is present, released, transmitted
through the null output and echoed as our own line. The script streams
real-time silence after the packets, because the demodulator only drops its
carrier detect when audio keeps flowing.

To run the application itself without a sound card, point it at a
configuration with `ADEVICE null null` (Linux) and start it with the modem:

```sh
./build/ax25chat --modem-conf conf/test-null.conf --call F4TST-1
```

## How the embedding works

Dire Wolf is not a library. Its CMake compiles about eighty C files straight
into the `direwolf` executable, and `direwolf.c` holds a thousand-line
`main()` doing all the initialisation. Embedding it meant keeping the modem
and protocol modules as they are and replacing exactly three points of
contact.

**`direwolf.c`** is not compiled. `dw_embed.c` runs the same initialisation
sequence as `main()` (`config_init`, `audio_open`, `multi_modem_init`,
`fx25_init`, `il2p_init`, `gen_tone_init`, `xmit_init`, `ax25_link_init`,
`recv_init`) and the same dispatch loop as `recv_process()`, with a stop flag
added. `app_process_rec_packet()`, which `recv.c` calls for every frame that
passed its FCS or FEC check and which the stand-alone program uses to print,
digipeat and feed KISS clients, now hands the raw frame to the host.
Transmission is `tq_append()` on the transmit queue, no KISS in between.

**`textcolor.c`** is not compiled. Every message Dire Wolf prints goes
through `text_color_set()` and `dw_printf()`; `dw_textcolor.c` assembles
complete lines per thread and routes them to a callback with the category
(`DW_COLOR_ERROR` and so on) that was in force. All console parsing of the
Python version — the readiness line, the version banner, the channel list,
the Windows stdout buffering problem — is gone with it.

**`exit()`**. The Dire Wolf sources contain 216 calls, most in the utility
programs, but some in the core: a bad configuration line, an audio device
that cannot be opened, an input stream that ends, a PTT device that is
missing. A library must not end the process, so `dw_embed_shim.h` is force
included into every Dire Wolf translation unit (`-include`, or `/FI` with
MSVC) and rewrites the token `exit` to `dw_embed_exit()`. That function
reports the reason to the host — the last line the calling thread printed,
which is nearly always the explanation — then unwinds only the calling
thread: `longjmp` back into `dw_embed_start()` or `dw_embed_run()` on the
engine thread, thread exit on a thread Dire Wolf created itself. Out of
memory and mutex failures still go through the same path; there is nothing
sensible to recover there, but the GUI at least gets to say why.

**Carrier sense.** `ptt_set()` in Dire Wolf raises a `DLQ_CHANNEL_BUSY` item
whenever the DCD of a channel changes or our own PTT is keyed. The dispatch
loop forwards those to the host, which therefore sees the modem's real
carrier detect — including noise that never becomes a valid frame, the case
the KISS-based version could not see at all. The transmit queue is queried
directly with `tq_count()`, which is what the KISS `TXBUF:` query used to
answer, without polling and without log spam.

### What is left out of the core

The KISS servers (TCP, serial, pseudo terminal), the AGW server, the IGate,
the digipeater, Dire Wolf's own beacons, GPS handling and waypoint output,
the APRStt gateway, log files, the heard list and the packet filters. The
handful of their functions that the remaining modules still reference are
no-ops in `dw_stubs.c`, with prototypes copied from the Dire Wolf headers so
that an upstream signature change is caught at compile time rather than at
run time. The connected-mode link layer (`ax25_link.c`) stays in because
`recv.c` dispatches to it; it is initialised and idle.

### The patch

`patches/direwolf/0001-embedded-host-shutdown.patch` adds `tq_term()` and
`recv_term()`. Stand-alone Dire Wolf never stops its transmit threads or its
audio input threads; it just exits. An embedded modem has to be stoppable —
that is how the application changes sound card or modem settings at run
time — so the transmit threads now poll a flag after each wake-up and the
input threads poll one after each audio period, and both return instead of
calling `exit()`. `tq_init()` and `recv_init()` clear the flags, which makes
a second start work. The Direwolf statics are otherwise re-initialised by
the same `*_init()` functions the program calls once.

### Configuration and data files

`direwolf.conf` remains the modem configuration: `config.c` is compiled
unchanged, so `ADEVICE`, `ARATE`, `ACHANNELS`, `CHANNEL`, `MYCALL`, `MODEM`,
`PTT`, `DCD`, `TXDELAY`, `PERSIST`, `SLOTTIME`, `TXTAIL`, `FULLDUP`, `FX25TX`,
`IL2PTX` and the rest keep their meaning and their documentation. Keywords
for the modules left out (`KISSPORT`, `AGWPORT`, `DIGIPEAT`, `IGSERVER`,
`PBEACON`, `GPSNMEA`, `LOGDIR`, ...) are still parsed but have no effect. The
KISS parameters can also be changed while running through
`DirewolfEngine::setChannelParams()`.

The core looks for `tocalls.yaml` (device identification from the
destination address) and `symbols-new.txt` (recent overlay symbols) in the
current directory or `data/`; without them it warns once and carries on. The
application should ship Dire Wolf's copies next to the executable.

## The protocol layer

`AX25Frame` is a port of `ax25.py`: addresses with their SSID and H bit,
UI frame construction, encoding to what the modem wants (no FCS), decoding
of any frame type for the monitor, TNC-2 rendering, and `splitMessage()`,
which fragments a chat message on whitespace without ever cutting a
multi-octet character. The FCS is Direwolf's `fcs_calc()`. The test suite
builds the same frames with Direwolf's `ax25_from_text()` and compares the
octets, so the address packing cannot drift from the C original.

One deliberate difference was found there: `ax25_from_text()` sets the C bit
in both the destination and the source address (`cc=11`, the pre-2.0 form
Direwolf uses for its own beacons), while this application keeps the AX.25
v2.2 command form (C=1 destination, C=0 source), as the Python version did.
Receivers ignore the C bits of UI frames, so nothing changes on the air.

`AprsDecoder` calls Direwolf's `decode_aprs()` for everything that carries a
position — uncompressed, compressed, with timestamp, `!DAO!`, objects,
items, Mic-E, third-party traffic — and for status, capabilities and
queries, then fills `AprsPacket`, whose fields are those of the Python
`DecodedPacket` plus what Direwolf adds (manufacturer, Maidenhead locator,
frequency, PHG range). Coordinates are decimal degrees, speed knots,
altitude metres. Two things are still done on this side: messages, because
the messaging layer needs the reply-ack forms `{12}` and `{12}34` as fields
and Direwolf only prints them; and weather, because Direwolf renders the
readings as imperial text and the station list wants metric numbers. The
weather parser keeps the Python rules: dots mean "no sensor", humidity `00`
is 100 %, and a leading `nnn/nnn` on a station whose symbol is `_` is wind,
not course and speed. The messages `decode_aprs()` prints about a packet
are captured per call and returned in `AprsPacket::errors`.

## The model layer

`AppConfig` keeps the file, the location and the keys of the Python
version, so an existing `config.json` loads unchanged — with one migration.
The Python version reached the modem through a KISS link and supervised an
external Direwolf, which took two sections, `tnc` and `direwolf`. Those
collapse into one, `modem`: the `direwolf.conf` to load, the radio channel,
whether to start at launch and echo the modem's log, and the channel access
parameters (TXDELAY, persistence, slot time, TX tail, full duplex). A file
still carrying the old sections is converted on load; the executable path,
arguments, timeouts and console options described a process that no longer
exists and are dropped. Unknown keys are ignored and values of the wrong
type keep their default, as before.

`ChannelManager` is the port of `channel.py` with one input added. The
Python version, a KISS host, could only infer occupancy from frames after
they had been demodulated; the manager now also receives the modem's carrier
detect through `setDcd()` and our own PTT through `setPtt()`, both from
`DirewolfEngine` signals, so a carrier counts as busy the moment it appears,
including noise that never becomes a valid frame. The quiet period after the
last activity, the random back-off, the inter-frame gap, the abandonment of
frames that never found a gap, and the "wait for the modem's transmit queue
to drain" rule are unchanged, except that the queue is read directly with
`txQueueBytes()` instead of polled over KISS. `use_dcd` in the channel
section turns the new input off. The class takes an injectable clock, and
the tests drive every timing rule without waiting for real time.

`SmartBeaconer` is a straight port of `smartbeacon.py`, including the clamp
at the low-speed boundary and the 180-degree cap on the turn threshold; the
expected intervals and thresholds in its tests were produced by the Python
module.

`MessageManager`, `StationRegistry`, `PeriodicSender` and `GpsReceiver` are
straight ports of `messaging.py`, `stations.py`, `beacon.py` and `gps.py`,
with the same identifiers, retry schedule (0s, 30s, 1m30, 3m30, 7m30, failed
at 8m00 with the defaults), duplicate suppression, bulletin rules, age
formatting, expiry, NMEA checksum and GGA/RMC merging. Each takes an
injectable clock so its tests run without waiting; `GpsReceiver::feedText()`
lets tests and replays push NMEA text through the same path as the serial
port.

## The application

`MainWindow` is the port of `main_window.py` with the modem in-process. The
toolbar's *Start Direwolf* and *Connect TNC* become one *Start modem*
action (still `Ctrl+K`); the status bar shows the modem's state and channel
in place of a link and a process. When the modem starts, the window checks
that the configured channel is one the configuration file declares, pushes
the channel access parameters, and puts the channel manager online; the
modem's carrier detect and PTT feed the channel manager directly. Frames
arrive as `DwReceivedFrame` from the engine and follow the same path as
before: AX.25 decode, APRS decode, station registry, message manager, then
chat line, direct line or monitor line. A modem fault (an audio device that
disappears, an input stream that ends) is reported in the log and leaves
the window usable; *Start modem* starts it again.

`SettingsPage` covers every field of the configuration. The Direwolf group
of the Python version is replaced by the Modem group: the `direwolf.conf`
to load with *Browse*, *Create* (a starter file in the settings folder, or
next to the path entered when that directory is writable) and *Edit* (the
system text editor), the radio channel — choosing a file reads its
`CHANNEL` lines and offers to align the setting — automatic start, log
echo, and the channel access parameters. The SmartBeaconing and retry
timetables are previewed as the numbers change, as before.

`main.cpp` keeps the command line of `run.py` minus the options that
described the KISS link and the external process: `--config`, `--call`,
`--modem-conf`, `--no-modem`, `--style`, `--list-styles`, `--version`. On
the first run, when no `config.json` exists and no `direwolf.conf` is found
in the usual places, a starter file is written to the settings folder and
the window says so.

## The Qt engine

`DirewolfEngine` is a `QObject` living in the GUI thread. `start(conf)`
launches the core on a `QThread`; the core's C callbacks copy their data and
queue it to the GUI thread, so every signal — `frameReceived`, `dcdChanged`,
`pttChanged`, `logMessage`, `faulted`, `started`, `startFailed`, `stopped` —
is delivered on the GUI thread and slots may touch widgets. Queries such as
`txQueueBytes()` and `dcd()` call the core directly and are safe from the
GUI thread. One instance per process; the core keeps its state in statics.

## Roadmap

The Python modules and where each one goes in the port:

| Python | C++ | Notes |
|---|---|---|
| `direwolf.py` (subprocess supervisor) | `DirewolfEngine` | done; executable detection, console capture, port probing, "own console window" no longer have an object |
| `kiss.py` (TCP / serial KISS) | — | removed: the modem is in-process; hardware TNCs are out of scope by decision |
| `ax25.py` | `AX25Frame` | done; FCS from `fcs_calc.c`, encoding checked against `ax25_pad.c` |
| `aprs_decode.py` | `AprsDecoder` over `decode_aprs.c` | done; messages and metric weather kept on this side |
| `channel.py` | `ChannelManager` | done; fed by real DCD and PTT from the engine, transmit queue read directly |
| `config.py` | `AppConfig` | done; `tnc` + `direwolf` sections migrated into `modem` |
| `aprs.py` | `AprsEncoder`, `PositionReport` | done; the symbol name tables move to the picker |
| `smartbeacon.py` | `SmartBeaconer` | done |
| `messaging.py` | `MessageManager` | done |
| `stations.py` | `StationRegistry` | done |
| `beacon.py` | `PeriodicSender` | done |
| `gps.py` | `GpsReceiver` on `QSerialPort` | done |
| `symbols.py` | `SymbolArt` | done |
| `notifications.py` | `Notifier` | done; `QSoundEffect` when Qt Multimedia is present, beep otherwise |
| `ui/main_window.py` | `MainWindow` | done |
| `ui/settings_page.py` | `SettingsPage` | done; Modem group replaces the Direwolf group |
| `ui/symbol_picker.py` | `SymbolPicker` | done |
| `ui/style.py` | `Style` | done |
| `run.py` | `main.cpp` | done; KISS and process options dropped |
| `installer.iss`, `build_windows.*`, `ax25chat.spec` | — | not ported: a CPack/NSIS or Inno Setup script for the C++ build is still to write |

Remaining: a macOS bundle, and a session on the air with a real sound card
and PTT line. The Windows
installer is built by `build_all.bat`; the core has been cross-compiled
for Windows with MinGW-w64 but not yet built with clang-cl on a Windows
machine, so the first `build_all.bat` run may still turn up something in
the bundled GNU regex or hidapi under the Microsoft runtime.

## Licensing

Dire Wolf is Copyright (C) 2011-2025 John Langner, WB2OSZ, licensed under
the GNU General Public License version 2 or, at your option, any later
version. This program compiles a modified copy of it — the modification is
`patches/direwolf/0001-embedded-host-shutdown.patch`, and `direwolf.c` and
`textcolor.c` are replaced by the files in `src/core` — and is therefore a
derived work distributed under the same terms, GPL-2.0-or-later. Qt is used
under the LGPL version 3, which is compatible with the GPL version 3 that the
"or later" clause permits.

Whoever distributes a binary must make the complete corresponding source
available, including the patched Dire Wolf tree; the earlier notice stating
that Dire Wolf "is a separate program, not modified in any way" no longer
applies and must not be shipped with this version.
