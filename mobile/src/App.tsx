import { useCallback, useState } from "react";
import type { BleDevice } from "@mnlphlp/plugin-blec";
import ConnectScreen from "./screens/ConnectScreen";
import ConsoleScreen from "./screens/ConsoleScreen";
import ModeScreen from "./screens/ModeScreen";
import StatusScreen from "./screens/StatusScreen";
import DumpScreen from "./screens/DumpScreen";
import AsyncScreen, { type AsyncRecord } from "./screens/AsyncScreen";
import ErrorBoundary from "./ErrorBoundary";
import { consumeLine } from "./session";
import type { Line } from "./types";

type Tab = "connect" | "console" | "mode" | "status" | "dump" | "async";
const TABS: { id: Tab; label: string }[] = [
  { id: "connect", label: "Connect" },
  { id: "console", label: "Console" },
  { id: "mode", label: "Mode" },
  { id: "status", label: "Status" },
  { id: "dump", label: "Dump" },
  { id: "async", label: "Async" },
];

const HISTORY_LIMIT = 500;
const ASYNC_LIMIT = 200;

export default function App() {
  const [tab, setTab] = useState<Tab>("connect");
  const [connected, setConnected] = useState<BleDevice | null>(null);
  const [history, setHistory] = useState<Line[]>([]);
  const [asyncEvents, setAsyncEvents] = useState<AsyncRecord[]>([]);

  const onLine = useCallback((line: Line) => {
    setHistory((prev) => {
      const next = [...prev, line];
      return next.length > HISTORY_LIMIT ? next.slice(-HISTORY_LIMIT) : next;
    });
    if (line.kind === "async") {
      setAsyncEvents((prev) => {
        const next = [...prev, { t: Date.now(), raw: line.text }];
        return next.length > ASYNC_LIMIT ? next.slice(-ASYNC_LIMIT) : next;
      });
    }
    consumeLine(line);
  }, []);

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
        <ErrorBoundary key={tab}>
          {tab === "connect" && (
            <ConnectScreen
              connected={connected}
              setConnected={setConnected}
              onLine={onLine}
            />
          )}
          {tab === "console" && (
            <ConsoleScreen history={history} connected={!!connected} />
          )}
          {tab === "mode" && <ModeScreen connected={!!connected} />}
          {tab === "status" && <StatusScreen connected={!!connected} />}
          {tab === "dump" && <DumpScreen connected={!!connected} />}
          {tab === "async" && <AsyncScreen events={asyncEvents} />}
        </ErrorBoundary>
      </div>
    </main>
  );
}
