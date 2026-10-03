cmake_minimum_required(VERSION 3.21)

project(AX25Chat VERSION 1.6.6 DESCRIPTION "Packet radio chat over AX.25 with an embedded Dire Wolf modem" LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Android's integer version, monotonic across releases: 1.1.0 -> 10100.
math(EXPR AX25CHAT_VERSION_CODE "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")

option(AX25CHAT_BUILD_TOOLS "Build the console tools (dwcore_test)" ON)
option(AX25CHAT_BUILD_ENGINE "Build the Qt engine and protocol libraries" ON)
option(AX25CHAT_BUILD_TESTS "Build the unit tests (needs Qt6::Test)" ON)
option(AX25CHAT_BUILD_APP "Build the Qt Widgets desktop application" ON)
option(AX25CHAT_BUILD_MOBILE "Build the Qt Quick interface (phones, tablets; also runs on the desktop)" ON)
option(AX25CHAT_PACKAGE_SYMBOLS "Install the aprs.fi symbol artwork when assets/aprs-symbols is present" ON)

# ---- Embedded Dire Wolf core --------------------------------------------
if(MSVC)
  # Microsoft's math.h only defines M_PI with this; clang-cl uses that math.h.
  # And the CRT's deprecation notices for strncpy and friends are noise here.
  add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # clang-cl links with lld-link.  Qt gives every executable a manifest
    # (longPathAware, asInvoker), which CMake hands to the linker as
    # /MANIFESTINPUT; lld-link then merges its own trustInfo block into it
    # and, in the versions shipped with Visual Studio, prefixes the merged
    # attributes with an XML namespace (ms_asmv1:level) that Windows rejects:
    # the program fails to start with "side-by-side configuration is
    # incorrect" (error 14001).  Qt's own fix, applied here as well: no
    # linker trustInfo block at all - the manifest already declares asInvoker.
    add_link_options(/MANIFESTUAC:NO)
  endif()
endif()

include(cmake/DirewolfCore.cmake)

# ---- Console harness, no Qt needed ---------------------------------------
if(AX25CHAT_BUILD_TOOLS)
  add_executable(dwcore_test src/tools/dwcore_test.cpp)
  target_link_libraries(dwcore_test PRIVATE direwolf_core)
endif()

# ---- Qt engine ------------------------------------------------------------
if(AX25CHAT_BUILD_ENGINE OR AX25CHAT_BUILD_APP OR AX25CHAT_BUILD_MOBILE)
  # SerialPort does not exist on Android, Positioning is an add-on module:
  # both optional, and OPTIONAL_COMPONENTS keeps their absence a status
  # line rather than a warning.
  find_package(Qt6 REQUIRED COMPONENTS Core)
  find_package(Qt6 QUIET OPTIONAL_COMPONENTS SerialPort Positioning)
  qt_standard_project_setup()

  add_library(ax25chat_engine STATIC
    src/engine/DirewolfEngine.h
    src/engine/DirewolfEngine.cpp
    src/engine/AndroidPtt.h
    src/engine/AndroidPtt.cpp
    src/engine/AndroidNotify.h
    src/engine/AndroidNotify.cpp
  )
  target_include_directories(ax25chat_engine PUBLIC src/engine)
  target_link_libraries(ax25chat_engine PUBLIC Qt6::Core direwolf_core)

  add_executable(dwengine_demo src/tools/dwengine_demo.cpp)
  target_link_libraries(dwengine_demo PRIVATE ax25chat_engine)

  # AX.25 frames and APRS decoding, over the Direwolf originals.
  add_library(ax25chat_protocol STATIC
    src/protocol/AX25Frame.h
    src/protocol/AX25Frame.cpp
    src/protocol/AprsPacket.h
    src/protocol/AprsDecoder.h
    src/protocol/AprsDecoder.cpp
    src/protocol/AprsEncoder.h
    src/protocol/AprsEncoder.cpp
  )
  target_include_directories(ax25chat_protocol PUBLIC src/protocol)
  target_link_libraries(ax25chat_protocol PUBLIC Qt6::Core direwolf_core)

  # Configuration and the application model (channel access, later the
  # messaging, beacon and station classes).
  add_library(ax25chat_model STATIC
    src/model/AppConfig.h
    src/model/AppConfig.cpp
    src/model/ChannelManager.h
    src/model/ChannelManager.cpp
    src/model/SmartBeacon.h
    src/model/SmartBeacon.cpp
    src/model/MessageManager.h
    src/model/MessageManager.cpp
    src/model/StationRegistry.h
    src/model/StationRegistry.cpp
    src/model/PeriodicSender.h
    src/model/PeriodicSender.cpp
    src/model/GpsReceiver.h
    src/model/GpsReceiver.cpp
    src/model/GpsProvider.h
    src/model/GpsProvider.cpp
    src/model/Session.h
    src/model/Session.cpp
    src/model/ModemConfigFile.h
    src/model/ModemConfigFile.cpp
    src/model/AprsIsClient.h
    src/model/AprsIsClient.cpp
    src/model/TileProviders.h
    src/model/TileProviders.cpp
    src/model/TileCache.h
    src/model/TileCache.cpp
  )
  target_include_directories(ax25chat_model PUBLIC src/model)
  find_package(Qt6 REQUIRED COMPONENTS Network)
  target_link_libraries(ax25chat_model PUBLIC Qt6::Core Qt6::Network ax25chat_protocol ax25chat_engine)
  target_compile_definitions(ax25chat_model PUBLIC AX25CHAT_VERSION="${PROJECT_VERSION}")
  if(Qt6SerialPort_FOUND)
    target_compile_definitions(ax25chat_model PUBLIC AX25CHAT_HAVE_SERIALPORT=1)
    target_link_libraries(ax25chat_model PUBLIC Qt6::SerialPort)
  else()
    message(STATUS "Qt6 SerialPort not found: serial GPS receivers are not available")
  endif()
  if(Qt6Positioning_FOUND)
    target_sources(ax25chat_model PRIVATE src/model/PhoneLocation.h src/model/PhoneLocation.cpp)
    target_compile_definitions(ax25chat_model PUBLIC AX25CHAT_HAVE_POSITIONING=1)
    target_link_libraries(ax25chat_model PUBLIC Qt6::Positioning)
  else()
    message(STATUS "Qt6 Positioning not found: the phone location source is not available")
  endif()

  if(AX25CHAT_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_executable(tst_protocol tests/tst_protocol.cpp)
    target_link_libraries(tst_protocol PRIVATE Qt6::Test ax25chat_protocol)
    add_test(NAME protocol COMMAND tst_protocol)
    add_executable(tst_model tests/tst_model.cpp)
    target_link_libraries(tst_model PRIVATE Qt6::Test ax25chat_model)
    add_test(NAME model COMMAND tst_model)
    add_executable(tst_aprs_encoder tests/tst_aprs_encoder.cpp)
    target_link_libraries(tst_aprs_encoder PRIVATE Qt6::Test ax25chat_model)
    add_test(NAME aprs_encoder COMMAND tst_aprs_encoder)
    add_executable(tst_messaging tests/tst_messaging.cpp)
    target_link_libraries(tst_messaging PRIVATE Qt6::Test ax25chat_model)
    add_test(NAME messaging COMMAND tst_messaging)
    add_executable(tst_aprsis tests/tst_aprsis.cpp)
    target_link_libraries(tst_aprsis PRIVATE Qt6::Test ax25chat_model)
    add_test(NAME aprsis COMMAND tst_aprsis)

    add_executable(tst_gps tests/tst_gps.cpp)
    target_link_libraries(tst_gps PRIVATE Qt6::Test ax25chat_model)
    add_test(NAME gps COMMAND tst_gps)
  endif()
endif()

if(AX25CHAT_BUILD_APP OR AX25CHAT_BUILD_MOBILE)
  # Widgets serves the desktop window, and the pieces the mobile interface
  # borrows (notifier, symbol tables and artwork).
  find_package(Qt6 REQUIRED COMPONENTS Widgets OPTIONAL_COMPONENTS Multimedia)

  add_library(ax25chat_ui STATIC
    src/ui/Style.h
    src/ui/Style.cpp
    src/ui/SymbolArt.h
    src/ui/SymbolArt.cpp
    src/ui/SymbolPicker.h
    src/ui/SymbolPicker.cpp
    src/ui/Notifier.h
    src/ui/Notifier.cpp
    src/ui/SettingsPage.h
    src/ui/SettingsPage.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/MapWidget.h
    src/ui/MapWidget.cpp
  )
  target_include_directories(ax25chat_ui PUBLIC src/ui)
  target_link_libraries(ax25chat_ui PUBLIC Qt6::Widgets ax25chat_model)
  if(WIN32)
    target_link_libraries(ax25chat_ui PRIVATE winmm)    # PlaySound, the notification without QtMultimedia
  endif()
  if(Qt6Multimedia_FOUND)
    target_compile_definitions(ax25chat_ui PUBLIC AX25CHAT_HAVE_MULTIMEDIA=1)
    target_link_libraries(ax25chat_ui PUBLIC Qt6::Multimedia)
  else()
    message(STATUS "Qt6 Multimedia not found: custom notification sounds fall back to the system beep")
  endif()

  # Where an installed program finds its data files and the symbol artwork
  # (main.cpp, SymbolArt.cpp); on Windows they sit next to the executable.
  include(GNUInstallDirs)
  target_compile_definitions(ax25chat_ui PUBLIC
    AX25CHAT_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/ax25chat")
endif()

# ---- the desktop application ----------------------------------------------------
if(AX25CHAT_BUILD_APP)
  qt_add_executable(ax25chat src/app/main.cpp)
  qt_add_resources(ax25chat "icons" PREFIX "/icons" BASE assets FILES assets/ax25chat.png)
  # The built-in notification chime (the system beep is silent on most desktops).
  qt_add_resources(ax25chat "sounds" PREFIX "/sounds" BASE assets/sounds FILES assets/sounds/notify.wav)
  if(WIN32)
    target_sources(ax25chat PRIVATE assets/ax25chat.rc)
  endif()
  set_target_properties(ax25chat PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
  target_link_libraries(ax25chat PRIVATE ax25chat_ui)

  # ---- install ---------------------------------------------------------------
  install(TARGETS ax25chat RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES ${DIREWOLF_DIR}/data/tocalls.yaml ${DIREWOLF_DIR}/data/symbols-new.txt
          DESTINATION ${CMAKE_INSTALL_DATADIR}/ax25chat/data)
  if(AX25CHAT_PACKAGE_SYMBOLS AND EXISTS ${CMAKE_SOURCE_DIR}/assets/aprs-symbols/aprs-symbols-64-0.png)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/aprs-symbols
            DESTINATION ${CMAKE_INSTALL_DATADIR}/ax25chat)
  endif()
  install(FILES README.md LICENSE.txt CHANGELOG.md
          DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ax25chat)
  if(UNIX AND NOT APPLE)
    set(BINDIR ${CMAKE_INSTALL_FULL_BINDIR})
    configure_file(packaging/desktop/ax25chat.desktop.in ${CMAKE_BINARY_DIR}/ax25chat.desktop @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/ax25chat.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(DIRECTORY packaging/icons/hicolor DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
    string(TIMESTAMP AX25CHAT_RELEASE_DATE "%Y-%m-%d" UTC)
    configure_file(packaging/appstream/io.github.ax25chat.AX25Chat.metainfo.xml.in
                   ${CMAKE_BINARY_DIR}/io.github.ax25chat.AX25Chat.metainfo.xml @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/io.github.ax25chat.AX25Chat.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    # Manual page and changelog, compressed as the policy asks (a native
    # package, so the file is changelog.gz, not changelog.Debian.gz).
    find_program(GZIP gzip)
    if(GZIP)
      add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ax25chat.1.gz
        COMMAND ${GZIP} -9 -n -c ${CMAKE_SOURCE_DIR}/packaging/man/ax25chat.1 > ${CMAKE_BINARY_DIR}/ax25chat.1.gz
        DEPENDS ${CMAKE_SOURCE_DIR}/packaging/man/ax25chat.1 VERBATIM)
      add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/changelog.gz
        COMMAND ${GZIP} -9 -n -c ${CMAKE_SOURCE_DIR}/packaging/deb/changelog > ${CMAKE_BINARY_DIR}/changelog.gz
        DEPENDS ${CMAKE_SOURCE_DIR}/packaging/deb/changelog VERBATIM)
      add_custom_target(compressed_docs ALL
        DEPENDS ${CMAKE_BINARY_DIR}/ax25chat.1.gz ${CMAKE_BINARY_DIR}/changelog.gz)
      install(FILES ${CMAKE_BINARY_DIR}/ax25chat.1.gz DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
      install(FILES ${CMAKE_BINARY_DIR}/changelog.gz DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ax25chat)
    endif()
    install(FILES packaging/deb/copyright DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ax25chat)
    # A literal path: Debian qualifies CMAKE_INSTALL_LIBDIR with the
    # architecture, and udev only looks in lib/udev/rules.d.
    install(FILES packaging/udev/99-ax25chat-cm108.rules DESTINATION lib/udev/rules.d)
  endif()

  # ---- Debian package ----------------------------------------------------------
  # Dependencies are not written by hand: dpkg-shlibdeps derives them from
  # the libraries actually linked, which gives the right list on Ubuntu as
  # on Raspberry Pi OS.
  if(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_NAME "ax25chat")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_CONTACT "AX25Chat Community <noreply@ax25chat.invalid>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Packet radio chat and APRS over AX.25 with a built-in modem")
    set(CPACK_PACKAGE_DESCRIPTION
"Chat over amateur packet radio in AX.25 UI frames, APRS messages with\n\
 acknowledgements and retries, decoding of positions, objects, status and\n\
 weather reports, position reporting from fixed coordinates or a GPS, and\n\
 periodic beacons. The modem is Dire Wolf, compiled into the program: a\n\
 sound card and a PTT line are all the hardware needed.")
    set(CPACK_STRIP_FILES ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pipewire-alsa | pulseaudio-utils")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/packaging/deb/postinst;${CMAKE_SOURCE_DIR}/packaging/deb/postrm")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
    set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
    include(CPack)
  endif()

  if(AX25CHAT_BUILD_TESTS)
    # End to end over the real modem: needs test audio on stdin, see tests/tst_app.cpp.
    add_executable(tst_app tests/tst_app.cpp)
    target_compile_definitions(tst_app PRIVATE TEST_CONF_DIR="${CMAKE_SOURCE_DIR}/conf")
    target_link_libraries(tst_app PRIVATE Qt6::Test ax25chat_ui)
    # The wrapper script generates the audio with gen_packets (Dire Wolf's
    # own build) and streams it; skipped when gen_packets is not on PATH.
    find_program(GEN_PACKETS gen_packets)
    if(GEN_PACKETS)
      add_test(NAME app COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_app_test.sh ${CMAKE_BINARY_DIR})
    endif()
  endif()
endif()

# ---- the Qt Quick interface ------------------------------------------------------
if(AX25CHAT_BUILD_MOBILE)
    find_package(Qt6 QUIET OPTIONAL_COMPONENTS Quick QuickControls2)
    if(Qt6Quick_FOUND AND Qt6QuickControls2_FOUND)
      # Android deployment paths are plain strings here; say so to Qt.
      if(QT_KNOWN_POLICY_QTP0002)
        qt_policy(SET QTP0002 NEW)
      endif()
      qt_add_executable(ax25chat-mobile
        src/mobile/main_mobile.cpp
        src/mobile/MobileApp.h src/mobile/MobileApp.cpp
        src/mobile/AndroidUi.h src/mobile/AndroidUi.cpp
        src/mobile/TileCache.h)
      set(AX25CHAT_QML_FILES
        src/mobile/qml/Main.qml
        src/mobile/qml/ChatPage.qml
        src/mobile/qml/StationsPage.qml
        src/mobile/qml/SettingsPage.qml
        src/mobile/qml/SettingsSectionPage.qml
        src/mobile/qml/SettingsStation.qml
        src/mobile/qml/SettingsConnection.qml
        src/mobile/qml/SettingsModem.qml
        src/mobile/qml/SettingsAprsIs.qml
        src/mobile/qml/SettingsPosition.qml
        src/mobile/qml/SettingsLocation.qml
        src/mobile/qml/SettingsPrivacy.qml
        src/mobile/qml/SettingsChat.qml
        src/mobile/qml/SettingsDisplay.qml
        src/mobile/qml/SettingsNotifications.qml
        src/mobile/qml/SettingsMap.qml
        src/mobile/qml/Field.qml
        src/mobile/qml/SubSection.qml
        src/mobile/qml/Note.qml
        src/mobile/qml/UpperField.qml
        src/mobile/qml/SymbolPage.qml
        src/mobile/qml/MapPage.qml)
      # The files sit in src/mobile/qml but must land at the module's root
      # (qrc:/qt/qml/AX25Chat/Mobile/Main.qml), hence the aliases.
      foreach(_qml ${AX25CHAT_QML_FILES})
        get_filename_component(_name ${_qml} NAME)
        set_source_files_properties(${_qml} PROPERTIES QT_RESOURCE_ALIAS ${_name})
      endforeach()
      qt_add_qml_module(ax25chat-mobile
        URI AX25Chat.Mobile
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        QML_FILES ${AX25CHAT_QML_FILES}
      )
      qt_add_resources(ax25chat-mobile "mobile_icons" PREFIX "/icons" BASE assets FILES assets/ax25chat.png)
      qt_add_resources(ax25chat-mobile "mobile_sounds" PREFIX "/sounds" BASE assets/sounds FILES assets/sounds/notify.wav)
      # Dire Wolf's data files, for a phone where nothing sits next to the
      # program: written to the data folder at start-up (MobileApp::prepareDataFolder).
      qt_add_resources(ax25chat-mobile "direwolf_data" PREFIX "/data" BASE ${DIREWOLF_DIR}/data
        FILES ${DIREWOLF_DIR}/data/tocalls.yaml ${DIREWOLF_DIR}/data/symbols-new.txt)
      # The aprs.fi symbol sheets, compiled in when they were fetched
      # (scripts/fetch_symbols.sh): a phone has no folder next to the
      # program to find them in.  Read assets/aprs-symbols/COPYRIGHT.md
      # before redistributing an APK that carries them.
      if(ANDROID AND AX25CHAT_PACKAGE_SYMBOLS AND EXISTS ${CMAKE_SOURCE_DIR}/assets/aprs-symbols/aprs-symbols-64-0.png)
        file(GLOB AX25CHAT_SYMBOL_SHEETS ${CMAKE_SOURCE_DIR}/assets/aprs-symbols/aprs-symbols-*.png)
        qt_add_resources(ax25chat-mobile "symbol_sheets" PREFIX "/aprs-symbols" BASE assets/aprs-symbols FILES ${AX25CHAT_SYMBOL_SHEETS})
        message(STATUS "Symbol artwork compiled into the mobile application")
      endif()
      set_target_properties(ax25chat-mobile PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
      target_link_libraries(ax25chat-mobile PRIVATE Qt6::Quick Qt6::QuickControls2 ax25chat_ui)
      if(ANDROID)
        # android/ holds UsbPtt.java and the launcher icon; the manifest is
        # Qt's own template, filled from these properties, and the
        # permissions are declared here rather than in a manifest copy that
        # would go stale with the next Qt.
        set_target_properties(ax25chat-mobile PROPERTIES
          QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
          QT_ANDROID_PACKAGE_NAME "org.ax25chat"
          QT_ANDROID_APP_NAME "AX25Chat"
          QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
          QT_ANDROID_VERSION_CODE "${AX25CHAT_VERSION_CODE}"
          QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
          QT_ANDROID_MIN_SDK_VERSION 28
          QT_ANDROID_TARGET_SDK_VERSION 35)
        # HTTPS on Android needs OpenSSL inside the APK: Qt Network's TLS
        # backend loads libssl_3/libcrypto_3 at run time and Qt does not ship
        # them.  Without them every https request fails silently - the map
        # tiles, for one.  KDAB keeps prebuilt libraries for every ABI.
        include(FetchContent)
        FetchContent_Declare(android_openssl
          GIT_REPOSITORY https://github.com/KDAB/android_openssl.git
          GIT_TAG master
          GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(android_openssl)
        if(EXISTS ${android_openssl_SOURCE_DIR}/android_openssl.cmake)
          include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
          add_android_openssl_libraries(ax25chat-mobile)
          message(STATUS "OpenSSL for Android bundled from ${android_openssl_SOURCE_DIR}")
        else()
          message(WARNING "android_openssl.cmake not found: HTTPS (map tiles) will not work on the phone")
        endif()
        if(COMMAND qt_add_android_permission)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.RECORD_AUDIO)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.MODIFY_AUDIO_SETTINGS)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.ACCESS_FINE_LOCATION)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.ACCESS_COARSE_LOCATION)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.WAKE_LOCK)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.POST_NOTIFICATIONS)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.INTERNET)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.ACCESS_NETWORK_STATE)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.FOREGROUND_SERVICE)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.FOREGROUND_SERVICE_MICROPHONE)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.FOREGROUND_SERVICE_LOCATION)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.FOREGROUND_SERVICE_DATA_SYNC)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.FOREGROUND_SERVICE_SPECIAL_USE)
          qt_add_android_permission(ax25chat-mobile NAME android.permission.REQUEST_IGNORE_BATTERY_OPTIMIZATIONS)
        else()
          message(WARNING "This Qt has no qt_add_android_permission(); add RECORD_AUDIO and the location permissions to the manifest by hand.")
        endif()
      endif()
      if(AX25CHAT_BUILD_TESTS)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_executable(tst_mobile tests/tst_mobile.cpp
          src/mobile/MobileApp.h src/mobile/MobileApp.cpp
          src/mobile/TileCache.h
          src/mobile/AndroidUi.h src/mobile/AndroidUi.cpp)
        target_include_directories(tst_mobile PRIVATE src/mobile)
        target_link_libraries(tst_mobile PRIVATE Qt6::Test Qt6::Quick ax25chat_ui)
        add_test(NAME mobile COMMAND tst_mobile)
        set_tests_properties(mobile PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        # The QML under real pointer events, loaded from the source tree (the
        # module is compiled into the application, not into a library).
        add_executable(tst_mobile_ui tests/tst_mobile_ui.cpp
          src/mobile/MobileApp.h src/mobile/MobileApp.cpp
          src/mobile/TileCache.h
          src/mobile/AndroidUi.h src/mobile/AndroidUi.cpp)
        target_include_directories(tst_mobile_ui PRIVATE src/mobile)
        target_compile_definitions(tst_mobile_ui PRIVATE AX25CHAT_QML_DIR="${CMAKE_SOURCE_DIR}/src/mobile/qml")
        target_link_libraries(tst_mobile_ui PRIVATE Qt6::Test Qt6::Quick Qt6::QuickControls2 ax25chat_ui)
        add_test(NAME mobile_ui COMMAND tst_mobile_ui)
        set_tests_properties(mobile_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
      endif()
    else()
      message(STATUS "Qt6 Quick not found: the mobile interface is not built")
    endif()
endif()

