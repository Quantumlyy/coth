import { useCallback, useEffect, useState } from "react";
import type { BleDevice } from "@mnlphlp/plugin-blec";
import {
  ADVERTISED_NAME,
  connect,
  disconnect,
  ensurePermissions,
  scan,
  subscribeNus,
  unsubscribeNus,
} from "../ble";
import type { Line } from "../types";

interface Props {
  connected: BleDevice | null;
  setConnected: (d: BleDevice | null) => void;
  onLine: (line: Line) => void;
}

export default function ConnectScreen({ connected, setConnected, onLine }: Props) {
  const [devices, setDevices] = useState<BleDevice[]>([]);
  const [scanning, setScanning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [manualAddr, setManualAddr] = useState("");

  useEffect(() => {
    ensurePermissions().catch((e) => setError(String(e)));
  }, []);

  const doScan = useCallback(async () => {
    setError(null);
    setScanning(true);
    try {
      const found = await scan(6000);
      // Surface devices named "Mellon" first, but show all so the operator
      // can pick a renamed unit.
      found.sort((a, b) =>
        Number(b.name === ADVERTISED_NAME) - Number(a.name === ADVERTISED_NAME),
      );
      setDevices(found);
    } catch (e) {
      setError(String(e));
    } finally {
      setScanning(false);
    }
  }, []);

  const doConnect = useCallback(
    async (dev: BleDevice) => {
      setError(null);
      try {
        await connect(dev.address, () => setConnected(null));
        await subscribeNus(onLine);
        setConnected(dev);
      } catch (e) {
        setError(String(e));
      }
    },
    [setConnected, onLine],
  );

  const doConnectManual = useCallback(async () => {
    if (!manualAddr.trim()) return;
    await doConnect({ address: manualAddr.trim(), name: "(manual)" } as BleDevice);
  }, [manualAddr, doConnect]);

  const doDisconnect = useCallback(async () => {
    try {
      await unsubscribeNus();
    } catch {
      /* ignore — already gone */
    }
    await disconnect();
    setConnected(null);
  }, [setConnected]);

  if (connected) {
    return (
      <section className="screen">
        <h2>Connected</h2>
        <p>
          <strong>{connected.name || "(unnamed)"}</strong>
          <br />
          <code>{connected.address}</code>
        </p>
        <button onClick={doDisconnect}>Disconnect</button>
      </section>
    );
  }

  return (
    <section className="screen">
      <h2>Scan</h2>
      <button onClick={doScan} disabled={scanning}>
        {scanning ? "scanning…" : "Scan for Mellon"}
      </button>
      {error && <p className="error">{error}</p>}

      <ul className="device-list">
        {devices.map((d) => (
          <li key={d.address}>
            <button onClick={() => doConnect(d)}>
              <strong>{d.name || "(unnamed)"}</strong>
              <code>{d.address}</code>
            </button>
          </li>
        ))}
      </ul>

      <details>
        <summary>Connect manually</summary>
        <input
          type="text"
          placeholder="AA:BB:CC:DD:EE:FF"
          value={manualAddr}
          onChange={(e) => setManualAddr(e.target.value)}
        />
        <button onClick={doConnectManual}>Connect</button>
      </details>
    </section>
  );
}
