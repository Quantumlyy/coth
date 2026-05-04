# Mellon mobile companion

Tauri 2 mobile app (iOS + Android) that talks to the Mellon firmware over
Nordic UART Service. The transport and command grammar are documented in
[`../docs/ble_protocol.md`](../docs/ble_protocol.md).

The app has six tabs:

| Tab | What it does |
|-----|--------------|
| Connect | Scan for advertised name `Mellon`, list nearby devices, connect, pair (passkey). |
| Console | Free-form REPL — type a command, see the device's reply. |
| Mode | One-tap switcher: `idle` / `sniff` / `keyset_watch` / `weak_keys`. |
| Status | Refresh button — issues `status` and renders the parsed block. |
| Dump | `dump <n>` and `preserved` viewers; renders typed dump rows. |
| Async | Live feed of `* KEYSET …` / `* MATCH …` / `* ARMED expired` notifications. |

## Layout

```
mobile/
  crates/mellon-proto/      # pure-Rust NUS line-protocol parser, host-tested
  src-tauri/                # Tauri 2 shell — registers tauri-plugin-blec and
                            #   exposes parser commands to the webview
  src/                      # React + TS frontend
    ble.ts                  #   plugin-blec wrapper + line framer
    session.ts              #   command dispatch (terminator + quiet-window)
    screens/                #   one component per tab
```

## First-run setup

```sh
cd mobile
npm install

# generate Xcode + Gradle projects under src-tauri/gen — one-time, machine-local
npm run tauri ios init
npm run tauri android init
```

`gen/{apple,android}` is committed (Tauri's documented stance), but build
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

Bump `minSdkVersion` to 24 in `src-tauri/gen/android/app/build.gradle.kts`
(tauri-plugin-blec floor).

## Running on a real device

```sh
# iOS — requires Xcode + a signing identity. Real device only — the
# iOS Simulator cannot do BLE.
npm run tauri ios dev

# Android — requires the Android SDK + a real phone. Most emulators do
# not implement BLE; if yours does, you can try.
npm run tauri android dev
```

### iOS pairing UX

The OS owns the passkey sheet; the plugin does not. On the first write
to the NUS RX characteristic after a fresh pairing, iOS pops the
six-digit passkey prompt (`123456` by default) and the write fails with
"insufficient auth". `sendCommand` in `src/ble.ts` catches that error
and retries once after 500 ms, so by the time the operator dismisses
the prompt the second attempt succeeds.

If pairing has been previously cleared (Settings → Bluetooth → Forget
Device), the prompt re-appears.

### Android 12+ permissions

`BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT` are runtime permissions. The
Connect screen calls `checkPermissions()` from `@mnlphlp/plugin-blec`
on mount so the OS prompt fires before the first scan — otherwise the
scan returns silently empty.

On Android 11 and earlier the legacy `ACCESS_FINE_LOCATION` permission
is still required (the manifest gates it with `android:maxSdkVersion="30"`).

## Host tests

The protocol layer is pure Rust with no I/O so it is fully host-testable
without a phone or any device:

```sh
cd mobile
cargo test -p mellon-proto
```

20+ unit tests plus golden-file integration tests cover the framer, the
dump-line parser, the status block parser, the keys row parser, and the
async-event classifier. Fixtures are taken verbatim from
[`../docs/ble_protocol.md`](../docs/ble_protocol.md) examples.

## Type-checking the frontend

```sh
cd mobile
npx tsc --noEmit
```
