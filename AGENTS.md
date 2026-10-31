# AGENTS

This file defines specialized agent roles used during development.

## Agent policy

1. Use focused agents for high-risk tasks to reduce context drift.
2. Keep isolation-critical reviews separate from feature coding runs.
3. Record assumptions and evidence in docs for each completed milestone.

## Agent roster

1. Isolation Guardian
- Purpose: review memory, interrupt, and scheduling changes.

2. Platform Bring-up Planner
- Purpose: coordinate enablement of the CIX boards listed in the workspace
  `.claude/rules/boards.md`. Each stays feasibility-gated (`unknown` or
  `BLOCKED`) until board evidence exists, and this role claims no board
  support. i.MX95 material is historical.

3. Thesis Evidence Curator
- Purpose: keep chapter traceability and evaluation assets consistent.

## Invocation guidance

These roles are review passes, not Claude Code subagents. Definitions for
another harness live in `.github/agents/`. Apply a role by task intent, not by
file type only.

1. Run an Isolation Guardian review before merging core control-path changes.
2. Run a Platform Bring-up review for all board config additions.
3. Run a Thesis Evidence review at each roadmap milestone boundary.
