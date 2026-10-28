---
description: Verify changes with CMake build, check git diff, format, and commit using Conventional Commits
agent: build
---

Execute the verification and commit pipeline for: $ARGUMENTS

Steps to execute:
1. Format touched C files according to Linux kernel style:
   `find src -name "*.c" -o -name "*.h" | xargs clang-format -i`
2. Run build verification:
   `cmake --build build`
3. Inspect touched files:
   `git status`
   `git diff`
4. Stage only relevant changes:
   `git add <files>`
5. Create a Conventional Commit with message format:
   `<type>(<scope>): <short summary in imperative mood>`
   (Types: feat, fix, refactor, style, test, chore, docs)
