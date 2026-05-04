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
