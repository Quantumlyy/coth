# Mellon mobile companion

Tauri 2 mobile app (iOS + Android) that talks to the Mellon firmware over
Nordic UART Service. The transport and command grammar are documented in
[`../docs/ble_protocol.md`](../docs/ble_protocol.md).

This is a skeleton — the BLE plug-in, screens, and parsers are introduced
in subsequent commits.

## First-run setup

```sh
# install JS deps
npm install

# generate Xcode + Gradle projects under src-tauri/gen (one-time, machine-local)
npm run tauri ios init
npm run tauri android init
```

`gen/{apple,android}` is committed (Tauri's documented stance) but the build
artefacts beneath are gitignored.

After init you must add BLE permissions before the first build:

**iOS** — `src-tauri/gen/apple/<app>/Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Mellon connects to your hardware over Bluetooth Low Energy.</string>
```

**Android** — `src-tauri/gen/android/app/src/main/AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation"/>
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT"/>
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"
    android:maxSdkVersion="30"/>
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true"/>
```

Bump `minSdkVersion` to 24 in `gen/android/app/build.gradle.kts` (tauri-plugin-blec floor).

## Running on a device

```sh
# iOS — requires Xcode + signing identity. Simulator cannot do BLE.
npm run tauri ios dev

# Android — requires Android SDK + a real phone (most emulators can't do BLE).
npm run tauri android dev
```

## Host tests

The protocol layer is pure Rust with no I/O so it is fully host-testable:

```sh
cargo test -p mellon-proto
```

## Layout

```
mobile/
  crates/mellon-proto/      # pure-Rust NUS line-protocol parser
  src-tauri/                # Tauri 2 shell (BLE + commands + events)
  src/                      # React + TS frontend
```
