# Contributing

Thanks for contributing to haven.

## Workflow

- Open an issue describing motivation and scope.
- Submit pull requests with clear commit history.
- Keep changes focused and include tests/documentation updates.

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

## AI workflow conventions

- Read AGENTS.md for role-specific agent behavior.
- Use .github/skills/thesis-evidence/SKILL.md at milestone boundaries.
- Keep chapter traceability current for every completed milestone.
