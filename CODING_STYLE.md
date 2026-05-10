# Coding Style

## C and low-level code

- Follow Linux-style C conventions where practical.
- Use tabs (8-column) for C, headers, and assembly.
- Keep functions small and explicit in purpose.
- Avoid dynamic allocation in core runtime paths.
- No floating-point use in deterministic scheduling paths.
- Document lock ordering and timing-critical sections.

## Documentation

- Keep architectural claims linked to measurable tests.
- State assumptions and non-goals explicitly.
