# Codex Project Memory

This directory is the durable project-memory store for Codex work on `PlantVsZombies`.

It was migrated on 2026-07-17 from the Claude Code project key `D--PVZ-PlantsVsZombies-PlantVsZombies`. The migration copied all 66 source Markdown files (288,537 bytes), verified every initial copy by SHA-256, and then converted 165 Claude-style aliases and wiki links into portable relative Markdown links. No engineering prose or metadata was intentionally rewritten. The original hashes are recorded in `SOURCE_SHA256SUMS.txt`.

`MEMORY.md` is the routing index; the remaining migrated Markdown files are focused project, feedback, or reference memories.

## Usage contract

1. Search `MEMORY.md` for the subsystem or task topic.
2. Read only the linked topic files needed for the current task.
3. Verify historical status claims against the current Git state, source, build, and tests.
4. Update the relevant topic file and its index entry when new work makes a memory stale.
5. Keep durable mandatory rules in the repository's `AGENTS.md`; keep detailed historical context here.

The old `~/.claude/projects/.../memory/` directory may remain as a backup, but it is no longer the authoritative copy.
