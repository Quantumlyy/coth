// Mellon BLE transport — wraps @mnlphlp/plugin-blec and frames NUS lines.
//
// The Mellon firmware exposes Nordic UART Service. Bytes flow:
//   write commands  → NUS_RX_UUID
//   read responses  ← NUS_TX_UUID (notifications)
// The wire format is line-oriented ASCII, terminated by \n. See
// docs/ble_protocol.md for the full grammar.

import {
  BleDevice,
  checkPermissions,
  connect as blecConnect,
  disconnect as blecDisconnect,
  sendString,
  startScan,
  stopScan,
  subscribeString,
  unsubscribe,
} from "@mnlphlp/plugin-blec";
import type { Line } from "./types";

export const NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
export const NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
export const NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

export const ADVERTISED_NAME = "Mellon";

export async function ensurePermissions(): Promise<void> {
  const granted = await checkPermissions(true);
  if (!granted) {
    throw new Error("Bluetooth permission denied");
  }
}

export async function scan(timeoutMs = 8000): Promise<BleDevice[]> {
  await ensurePermissions();
  return new Promise((resolve, reject) => {
    let latest: BleDevice[] = [];
    const timer = window.setTimeout(() => {
      stopScan().catch(() => {});
      resolve(latest);
    }, timeoutMs);
    startScan((devs) => {
      latest = devs;
    }, timeoutMs).catch((err) => {
      window.clearTimeout(timer);
      reject(err);
    });
  });
}

export async function connect(
  address: string,
  onDisconnect?: () => void,
): Promise<void> {
  await blecConnect(address, onDisconnect ?? (() => {}));
}

export async function disconnect(): Promise<void> {
  await blecDisconnect();
}

// On iOS the first write after a fresh pairing can fail while the OS
// passkey sheet is still up. One retry after a short delay covers the
// common case.
export async function sendCommand(line: string): Promise<void> {
  const payload = line.endsWith("\n") ? line : line + "\n";
  try {
    await sendString(NUS_RX_UUID, payload, "withResponse", NUS_SERVICE_UUID);
  } catch {
    await new Promise((r) => setTimeout(r, 500));
    await sendString(NUS_RX_UUID, payload, "withResponse", NUS_SERVICE_UUID);
  }
}

let lineBuf = "";

function classify(text: string): Line {
  if (text.startsWith("*")) {
    return { kind: "async", text: text.slice(1).trimStart() };
  }
  if (text === "ok") return { kind: "ok", detail: "" };
  if (text.startsWith("ok ")) return { kind: "ok", detail: text.slice(3) };
  if (text.startsWith("err:")) {
    return { kind: "err", reason: text.slice(4).trim() };
  }
  return { kind: "sync", text };
}

export async function subscribeNus(onLine: (line: Line) => void): Promise<void> {
  await subscribeString(NUS_TX_UUID, (chunk: string) => {
    lineBuf += chunk;
    let nl;
    while ((nl = lineBuf.indexOf("\n")) >= 0) {
      let line = lineBuf.slice(0, nl);
      lineBuf = lineBuf.slice(nl + 1);
      if (line.endsWith("\r")) line = line.slice(0, -1);
      onLine(classify(line));
    }
  });
}

export async function unsubscribeNus(): Promise<void> {
  await unsubscribe(NUS_TX_UUID);
  lineBuf = "";
}
