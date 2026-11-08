# Contributing

Thanks for contributing to haven.

## Workflow

- Open an issue describing motivation and scope.
- Submit pull requests with clear commit history.
- Keep changes focused and include tests/documentation updates.
- `main` is protected: changes reach it only through a pull request whose
  CI checks all pass (host unit tests on Linux, macOS and Windows; ARM64
  build and QEMU boot). Nobody can push to it directly or force-push, and
  nobody bypasses the rule. Pull requests are merged with a merge commit,
  so commit hashes cited in the research records stay valid.

## Sign-off

Use DCO sign-off on commits:

- git commit -s -m "message"

## Review expectations

- Isolation-sensitive changes require design notes.
- New platform support must include configuration and validation updates.
- Performance-affecting changes must include benchmark evidence.

## Local hooks setup

Enable repository-managed hooks:

- ./scripts/setup-hooks.sh

Hooks enforce:
- pre-commit: style and config checks
- commit-msg: conventional subject prefixes
- pre-push: build and test verification

## Traceability

- Keep chapter traceability current for every completed milestone.
