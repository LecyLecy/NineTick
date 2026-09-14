# Agent Instructions for Timer

## Ponytail

Use the local `skills/ponytail` skill when a task benefits from a deliberately
minimal, practical implementation. Prefer the smallest maintainable solution,
avoid speculative abstractions, and keep dependencies low. Use
`skills/ponytail-review` to check for avoidable complexity before a substantial
feature is handed over.

## Caveman

Use the local `skills/caveman` skill when concise, direct communication is
appropriate. Preserve exact code, commands, paths, diagnostics, and safety
warnings. Do not use compressed/caveman wording for irreversible actions,
security guidance, or multi-step instructions where clarity matters more than
brevity. The skill can be invoked explicitly as `$caveman` when supported.

## Project priorities

- Keep the desktop timer dependency-free where feasible and minimise idle CPU,
  memory, disk I/O, and startup work.
- Verify Windows behaviour after changes, including clean exit and persistence
  of the chosen screen position.
