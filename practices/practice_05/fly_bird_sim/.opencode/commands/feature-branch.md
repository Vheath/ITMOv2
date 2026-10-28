---
description: Create and switch to a new feature branch following standard branch conventions
agent: build
---

Switch to or create a feature branch for: $ARGUMENTS

Convention:
- Feature: `feat/<feature-name>`
- Fix: `fix/<bug-name>`
- Refactor: `refactor/<scope-name>`

Run git checkout:
```bash
git checkout -b "$ARGUMENTS"
```
