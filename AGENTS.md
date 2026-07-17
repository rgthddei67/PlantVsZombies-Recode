# AGENTS.md

Always-on rules for Codex work in this repository. Keep this file concise; load detailed guidance only through the routes below.

## Task routing

- Before building, running, using AutoTest, or changing architecture/resources/save behavior, read the relevant section of `docs/agent-guide/PROJECT_GUIDE.md`.
- For an existing subsystem or prior decision, search `docs/agent-memory/MEMORY.md` and read only the linked topic files that match the task. Memory is historical context: verify dated status, paths, commits, and tests against the current repository.
- For any plant, particle effect, survival perk, or zombie work, use the matching skill under `.agents/skills/` and follow its `SKILL.md` completely.
- For a new classic plant, zombie, or projectile, consult `D:\PVZ\PlantsVsZombies.NET-master\Lawn_Shared\Lawn` before implementation. For animation problems, inspect the matching `resources/reanim/` file and the C# reference.
- If new work needs animation frame events, ask 主人 before adding them.

## Build and verification

- This is an x64-Windows C++17 CMake/vcpkg project. Codex may build autonomously.
- Before CMake, add the Visual Studio Installer directory to `PATH`, locate VS with `vswhere`, then import `VsDevCmd.bat -arch=x64 -no_logo`; the exact PowerShell sequence is in the project guide.
- Release verification uses `cmake --preset clang-release` then `cmake --build --preset clang-release`. Use `msvc-debug` for debug/F5 work; there is no MSVC Release preset.
- Run from `build\<preset>\`; the executable is `build\<preset>\PlantsVsZombies.exe`. Never use the stale root `x64\Release` artifacts.
- For gameplay changes, run the narrowest relevant `-AutoTest` script from the build directory and inspect its `run.log`, state, and screenshots as appropriate. Documentation-only changes do not require a game build.

## Repository rules

- `GLOB_RECURSE CONFIGURE_DEPENDS` collects sources; new `.cpp` files need no manual build-list edit.
- Every new `.h` must start with `#pragma once`; the pre-commit hook enforces this.
- Keep Chinese text UTF-8. Keep logical grid position separate from visual offsets (`mVisualOffset`).
- Current task instructions, current source/Git state, and current build/test evidence override historical memory notes.
- After materially changing a documented subsystem, update its topic file and the corresponding entry in `docs/agent-memory/MEMORY.md`.

## Git and communication

- Codex commits completed, verified work, then decides whether to push from the current risk and repository state.
- Push when the work is complete, verified, scoped to the task, the intended upstream is unambiguous, and the push is a routine fast-forward. Otherwise keep the commit local and report why.
- Never force-push, rewrite published history, or publish unrelated or sensitive changes without explicit approval. An explicit instruction from 主人 always wins.
- Always address the user as **主人**.
