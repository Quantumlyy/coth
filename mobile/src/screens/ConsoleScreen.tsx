import { useEffect, useRef, useState } from "react";
import { sendCommand } from "../ble";
import type { Line } from "../types";

interface Props {
  history: Line[];
  connected: boolean;
}

function format(line: Line): string {
  switch (line.kind) {
    case "sync":
      return line.text;
    case "async":
      return "* " + line.text;
    case "ok":
      return line.detail ? "ok " + line.detail : "ok";
    case "err":
      return "err: " + line.reason;
  }
}

export default function ConsoleScreen({ history, connected }: Props) {
  const [input, setInput] = useState("");
  const [busy, setBusy] = useState(false);
  const logRef = useRef<HTMLPreElement | null>(null);

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [history.length]);

  async function submit() {
    const cmd = input.trim();
    if (!cmd) return;
    setInput("");
    setBusy(true);
    try {
      await sendCommand(cmd);
    } finally {
      setBusy(false);
    }
  }

  return (
    <section className="screen console">
      <pre className="log" ref={logRef}>
        {history.length === 0
          ? "(no traffic yet)"
          : history.map((line, i) => (
              <span key={i} className={`log-line log-${line.kind}`}>
                {format(line)}
                {"\n"}
              </span>
            ))}
      </pre>
      <form
        className="console-form"
        onSubmit={(e) => {
          e.preventDefault();
          submit();
        }}
      >
        <input
          type="text"
          autoCapitalize="none"
          autoCorrect="off"
          spellCheck={false}
          placeholder={connected ? "command (e.g. help)" : "not connected"}
          value={input}
          disabled={!connected || busy}
          onChange={(e) => setInput(e.target.value)}
        />
        <button type="submit" disabled={!connected || busy || !input.trim()}>
          send
        </button>
      </form>
    </section>
  );
}
