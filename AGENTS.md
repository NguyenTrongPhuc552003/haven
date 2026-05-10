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
- Purpose: coordinate i.MX95 and secondary platform enablement work.

3. Thesis Evidence Curator
- Purpose: keep chapter traceability and evaluation assets consistent.

## Invocation guidance

1. Trigger agents by task intent, not by file type only.
2. Use Isolation Guardian before merging core control-path changes.
3. Use Platform Bring-up Planner for all board config additions.
4. Use Thesis Evidence Curator at each roadmap milestone boundary.
