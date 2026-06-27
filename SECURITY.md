# Security Policy

## Supported Versions

Security fixes target the current V3 line.

| Version | Supported |
| ------- | --------- |
| 3.x     | Yes       |
| < 3.0   | No        |

## Reporting A Vulnerability

Please do not open a public issue with exploit details.

Use GitHub private vulnerability reporting if it is enabled for this repository.
If private reporting is not available, open a minimal public issue asking for a
private security contact and omit technical details until a private channel is
available.

Useful reports include:

- Affected version or commit.
- Minimal input that demonstrates the issue.
- Expected behavior and observed behavior.
- Compiler, platform, and sanitizer output when relevant.

The project is small and maintained on a best-effort basis. Security fixes are
prioritized when they affect strict parsing, memory safety, or allocator
boundaries.
