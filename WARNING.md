# WARNING — Authorized use only

The Mellon firmware in this repository is a security research tool. It implements
techniques against the OSDP (Open Supervised Device Protocol) protocol that were
publicly disclosed by Bishop Fox at Black Hat 2023.

## Using this firmware on systems you do not own or have written authorization to
## test is almost certainly illegal in your jurisdiction.

This includes, but is not limited to:

- Eavesdropping on OSDP buses in commercial buildings, transit systems, schools,
  hospitals, government facilities, or any third-party premises.
- Decrypting Secure Channel traffic captured from another party's bus, regardless
  of how easy the keys are to recover.
- Impersonating a Peripheral Device (PD) on any access-control bus you do not
  control. (Even though active-injection modes are not enabled in this build, the
  passive sniffer alone may still violate wiretapping statutes.)

The maintainers will not assist with, and will refuse to advise on, the use of
this tool against systems the operator cannot demonstrate authorization to test.

## Defensive use is encouraged

If you operate an OSDP system and want to assess whether it is exposed to the
attacks documented in spec §0:

- Run Mellon on a bench replica of your own infrastructure.
- Use it as a regression-detector in a lab as you migrate to OSDP Secure Channel
  with strong, randomly-generated SCBKs.
- Share findings with your reader/panel vendors so they can issue patches.

## No warranty

This is research-grade firmware. It will not be hardened against malicious peers,
will not gracefully recover from every bus fault, and may expose security-relevant
information (captured frames, recovered keys) over a BLE link that an attacker on
your side of the wall could also reach. Do not deploy it on a system where its
loss or compromise would matter.

See `LICENSE` for the (lack of) warranty in legal terms.
