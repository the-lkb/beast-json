# Security Policy

## Supported Versions

| Version | Supported |
|:--------|:----------|
| 1.0.x (current) | ✅ Active |
| < 1.0 | ❌ Not supported |

## Reporting a Vulnerability

**Please do not open a public GitHub Issue for security vulnerabilities.**

To report a security vulnerability, email **security@qbuem.com** with:

- A description of the vulnerability and its potential impact
- Steps to reproduce (a minimal test case is ideal)
- Which version(s) are affected
- Any suggested mitigations if you have them

We will acknowledge receipt within **48 hours** and aim to provide a resolution
timeline within **7 days**.  Critical vulnerabilities affecting memory safety
(buffer overflows, use-after-free) are prioritised for same-day acknowledgement.

## Disclosure Policy

We follow a **coordinated disclosure** model:

1. You report the issue privately.
2. We investigate and develop a fix.
3. We release a patched version and credit you (unless you prefer anonymity).
4. We publish a security advisory on GitHub after the patch is available.

We ask that you give us **90 days** from acknowledgement before public
disclosure, in line with industry-standard responsible disclosure practices.

## Scope

In scope:

- Memory safety issues in the parser or serializer (heap/stack overflows,
  use-after-free, out-of-bounds reads/writes)
- Undefined behaviour that could be exploited (integer overflow, misaligned
  access, type punning violations)
- Denial-of-service via algorithmic complexity on adversarial input
- Incorrect RFC 8259 / RFC 6901 / RFC 6902 compliance that could lead to
  security-relevant misinterpretation

Out of scope:

- Performance regressions without security impact
- Build system issues (CMake, FetchContent)
- Documentation errors

## Security Hardening

qbuem-json is continuously tested for memory safety:

- **AddressSanitizer (ASan)** — heap/stack overflow, use-after-free, memory leaks
- **UndefinedBehaviorSanitizer (UBSan)** — integer overflow, null deref, misaligned
  access, invalid enum
- **ThreadSanitizer (TSan)** — data races, lock-order inversions

These run on every pull request and push to `main` via the
[Sanitizers CI workflow](https://github.com/qbuem/qbuem-json/actions/workflows/sanitizers.yml).

Three [libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets fuzz the parser
continuously:

- `fuzz_dom` — DOM parser with arbitrary input
- `fuzz_parse` — Nexus / struct-mapping path
- `fuzz_rfc8259` — strict RFC 8259 parser

## Attribution

Security researchers who responsibly disclose vulnerabilities will be credited
in the release notes and, if they consent, in this file.
