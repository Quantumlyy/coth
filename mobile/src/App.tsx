import { useState } from "react";
import type { BleDevice } from "@mnlphlp/plugin-blec";
import ConnectScreen from "./screens/ConnectScreen";
import type { Line } from "./types";

type Tab = "connect";
const TABS: { id: Tab; label: string }[] = [{ id: "connect", label: "Connect" }];

export default function App() {
  const [tab, setTab] = useState<Tab>("connect");
  const [connected, setConnected] = useState<BleDevice | null>(null);

  // Subsequent commits add Console / Mode / Status / Dump / Async screens
  // that consume incoming lines. For now just print to the JS console so
  // the operator can verify NUS notifications flow during scaffolding.
  const onLine = (line: Line) => {
    console.log("[mellon]", line);
  };

  return (
    <main className="app">
      <header>
        <h1>Mellon</h1>
        <p className="muted">
          {connected ? `connected · ${connected.name || connected.address}` : "not connected"}
        </p>
      </header>

      <nav className="tabs">
        {TABS.map((t) => (
          <button
            key={t.id}
            className={t.id === tab ? "tab active" : "tab"}
            onClick={() => setTab(t.id)}
          >
            {t.label}
          </button>
        ))}
      </nav>

      <div className="screen-host">
        {tab === "connect" && (
          <ConnectScreen
            connected={connected}
            setConnected={setConnected}
            onLine={onLine}
          />
        )}
      </div>
    </main>
  );
}
