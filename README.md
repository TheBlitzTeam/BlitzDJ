# BlitzDJ

An intelligent DJ that uses the Spotify Web API to pick and queue music automatically.

BlitzDJ is a C++17 core with a terminal app for desktop, a native Android app, and an iOS XCFramework with a C API for Swift.

## Features

- Automatic Spotify login with OAuth PKCE (no client secret needed) and saved sessions
- AutoDJ that queues new tracks before the current one ends
- Ranking based on favorite artists, artist diversity and recent history
- Playback controls, search, devices and volume
- Automatic token refresh and retries for rate limits and network errors
- Bluetooth device management on Linux (BlueZ)

## Platforms

| Platform | Output | HTTP backend |
| --- | --- | --- |
| Windows, Linux, macOS | `BlitzDJ` terminal app | libcurl |
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
./build/BlitzDJ
```

Dependencies: CMake 3.21+, a C++17 compiler, libcurl, and `gio-2.0` on Linux for Bluetooth (`-DBLITZDJ_ENABLE_BLUETOOTH=OFF` to skip it).

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
