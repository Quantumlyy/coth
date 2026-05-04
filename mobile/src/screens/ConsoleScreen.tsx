import { useEffect, useRef, useState } from "react";
import { sendCommand } from "../ble";
import { onPendingChange } from "../session";
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
  const [structuredPending, setStructuredPending] = useState(false);
  const logRef = useRef<HTMLPreElement | null>(null);

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [history.length]);

  useEffect(() => onPendingChange(setStructuredPending), []);

  async function submit() {
    const cmd = input.trim();
    if (!cmd || structuredPending) return;
    setInput("");
    setBusy(true);
    try {
      await sendCommand(cmd);
    } finally {
      setBusy(false);
    }
  }

  const disabled = !connected || busy || structuredPending;

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
          placeholder={
            !connected
              ? "not connected"
              : structuredPending
                ? "another command is in flight…"
                : "command (e.g. help)"
          }
          value={input}
          disabled={disabled}
          onChange={(e) => setInput(e.target.value)}
        />
        <button type="submit" disabled={disabled || !input.trim()}>
          send
        </button>
      </form>
    </section>
  );
}
