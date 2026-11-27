# BlitzDJ

An intelligent DJ that uses the Spotify Web API to pick and queue music automatically.

BlitzDJ is a C++17 core with a desktop app (and a terminal app), a native Android app, and an iOS XCFramework with a C API for Swift.

## Features

- Automatic Spotify login with OAuth PKCE (no client secret needed) and saved sessions
- AutoDJ that queues new tracks before the current one ends
- Ranking based on favorite artists, artist diversity and recent history
- Playback controls, search, devices and volume
- Automatic token refresh and retries for rate limits and network errors
- Desktop app with the same design as Android (Dear ImGui + GLFW): album art, colors from the cover, AutoDJ, search and speakers
- Android app with Material 3 design, album art, dynamic colors, queue view and mini player
- Party mode on Android: keeps the screen awake and resumes music when a Bluetooth speaker reconnects
- Bluetooth speaker list, output switching and Spotify Connect devices on Android
- Proximity Bluetooth: nearby speakers sorted by signal strength, automatic connection to the closest paired speaker, and NFC tap to pair on Android
- Bluetooth device management on Linux (BlueZ)

## Platforms

| Platform | Output | HTTP backend |
| --- | --- | --- |
| Windows, Linux, macOS | `BlitzDJApp` desktop app and `BlitzDJ` terminal app | libcurl |
| Android | APK (`android/`) | `HttpURLConnection` through JNI |
| iOS | `BlitzDJ.xcframework` | `NSURLSession` |

## Spotify setup

1. Create an app at the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Add these redirect URIs:
   - Desktop: `http://127.0.0.1:8888/callback`
   - Android and iOS: `blitzdj://callback`
3. Copy the Client ID.

## Desktop

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBLITZDJ_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build
./build/BlitzDJApp
```

`BlitzDJApp` is the desktop app and `BlitzDJ` is the terminal version. Both share the same login session and settings.

Dependencies: CMake 3.21+, a C++17 compiler, libcurl, OpenGL, and on Linux `gio-2.0` for Bluetooth plus the X11 development packages (`xorg-dev libgl1-mesa-dev`). GLFW, Dear ImGui and stb are downloaded automatically. Use `-DBLITZDJ_BUILD_GUI=OFF` to build only the terminal app and `-DBLITZDJ_ENABLE_BLUETOOTH=OFF` to skip Bluetooth.

BlitzDJ ships with the TheBlitzTeam Client ID (`BLITZDJ_SPOTIFY_CLIENT_ID` in CMake, `spotifyClientId` in `android/gradle.properties`). Set `SPOTIFY_CLIENT_ID` to use your own Spotify app.

On first run BlitzDJ opens the Spotify login in your browser. A local server on `127.0.0.1:8888` receives the callback, so the connection finishes automatically. If that port is busy, BlitzDJ falls back to pasting the redirected URL. The session is saved, so you only log in once.

## Android

```bash
cd android
gradle assembleRelease -PspotifyClientId=YOUR_CLIENT_ID
```

The Client ID can also be changed in the app. When there is no saved session, the app opens the Spotify login automatically and finishes it through the `blitzdj://callback` link. AutoDJ runs in a foreground service, so it keeps queueing music with the screen off. Spotify must be open on some device (the phone itself works).

## iOS

The CI builds `BlitzDJ.xcframework`. Add it to an Xcode project and use the C API from Swift:

```swift
import BlitzDJ

let dj = blitzdj_create(clientId, "blitzdj://callback", sessionPath)
```

Register `blitzdj` as a URL scheme and pass the callback URL to `blitzdj_handle_redirect`. See `include/BlitzDJBridge.h` for the full API.

## License

MIT
