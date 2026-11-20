<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# Security

A DOTS reader handles untrusted input. A stream that makes a port
crash, hang, read or write out of bounds, or use resources beyond the
limits its caller set is a security issue.

## Reporting a vulnerability

Report it privately, through GitHub's private vulnerability reporting:
[open a report](https://github.com/PawelPajor/dots-codec/security/advisories/new).
Do not open a public issue.

Please include the port and its version, the input that triggers the
problem (a `.dots` file, or its octets), and what happens.

You will get an answer, and the fix is released for each affected
port. Credit is given in the advisory unless you ask otherwise.

## Supported versions

The latest release of each port.

## Not in scope

A stream that is not conforming DOTS and is rejected with an error is
not a vulnerability: rejecting it is the intended behaviour. A problem
in the specification itself belongs in
[dots-stream](https://github.com/PawelPajor/dots-stream).
