// LineFramer — splits a NUS byte stream on \n and classifies each line.
//
// Wire format (docs/ble_protocol.md):
//   - lines end with \n; \r is stripped
//   - lines beginning with `*` are asynchronous notifications
//   - `ok` (alone or `ok <descriptor>`) terminates a synchronous response
//   - `err: <reason>` terminates a synchronous response with failure
//   - everything else is a regular sync line

use serde::Serialize;

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Line {
    Sync { text: String },
    Async { text: String },
    Ok { detail: String },
    Err { reason: String },
}

#[derive(Debug, Default)]
pub struct LineFramer {
    buf: Vec<u8>,
}

impl LineFramer {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, data: &[u8]) -> Vec<Line> {
        self.buf.extend_from_slice(data);
        let mut out = Vec::new();
        while let Some(idx) = self.buf.iter().position(|&b| b == b'\n') {
            let raw = self.buf.drain(..=idx).collect::<Vec<_>>();
            // drop trailing \n and any trailing \r
            let end = raw.len().saturating_sub(1);
            let mut slice = &raw[..end];
            if slice.last() == Some(&b'\r') {
                slice = &slice[..slice.len() - 1];
            }
            let text = String::from_utf8_lossy(slice).into_owned();
            out.push(classify(text));
        }
        out
    }
}

fn classify(line: String) -> Line {
    if let Some(rest) = line.strip_prefix('*') {
        Line::Async {
            text: rest.trim_start().to_string(),
        }
    } else if line == "ok" {
        Line::Ok { detail: String::new() }
    } else if let Some(rest) = line.strip_prefix("ok ") {
        Line::Ok { detail: rest.to_string() }
    } else if let Some(rest) = line.strip_prefix("err:") {
        Line::Err { reason: rest.trim().to_string() }
    } else {
        Line::Sync { text: line }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn classifies_each_kind() {
        let mut f = LineFramer::new();
        let lines = f.push(b"hello\nok\nok mode=sniff\nerr: not found\n* KEYSET seen at t=1\n");
        assert_eq!(lines.len(), 5);
        assert_eq!(lines[0], Line::Sync { text: "hello".into() });
        assert_eq!(lines[1], Line::Ok { detail: "".into() });
        assert_eq!(lines[2], Line::Ok { detail: "mode=sniff".into() });
        assert_eq!(lines[3], Line::Err { reason: "not found".into() });
        assert_eq!(lines[4], Line::Async { text: "KEYSET seen at t=1".into() });
    }

    #[test]
    fn handles_split_chunks() {
        let mut f = LineFramer::new();
        assert!(f.push(b"hel").is_empty());
        assert!(f.push(b"lo\nwor").len() == 1);
        let last = f.push(b"ld\n");
        assert_eq!(last, vec![Line::Sync { text: "world".into() }]);
    }

    #[test]
    fn strips_trailing_cr() {
        let mut f = LineFramer::new();
        let lines = f.push(b"hello\r\nok\r\n");
        assert_eq!(lines[0], Line::Sync { text: "hello".into() });
        assert_eq!(lines[1], Line::Ok { detail: "".into() });
    }

    #[test]
    fn preserves_partial_line_across_pushes() {
        let mut f = LineFramer::new();
        assert!(f.push(b"ok wi").is_empty());
        let lines = f.push(b"ped\n");
        assert_eq!(lines, vec![Line::Ok { detail: "wiped".into() }]);
    }

    #[test]
    fn tolerates_non_utf8_bytes() {
        let mut f = LineFramer::new();
        let lines = f.push(b"hi \xff there\n");
        assert!(matches!(lines[0], Line::Sync { .. }));
    }
}
