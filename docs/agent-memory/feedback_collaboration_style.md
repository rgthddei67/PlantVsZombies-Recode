---
name: collaboration-style
description: "How this user wants perf/debugging work done — measure-first discipline, steady-state numbers, honest framing, VS build split, Chinese"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 7d6bf991-0204-4927-adcf-b4a0fd3c7e0b
---

# Collaboration style for this user

Rule: run a rigorous measure→experiment→decide loop. Don't propose or stack fixes on
unverified hypotheses; change one variable at a time; be honest when an experiment
falsifies your own theory (say so plainly, don't soften it — but also don't over-state
it; recalibrate when new context arrives).

**Why:** the user steered the whole PvZ perf investigation this way and pushed back
both when I guessed without evidence AND when I was over-pessimistic about a result.
They value intellectual honesty over performative progress.
**How to apply:**
- Always request STEADY-STATE / warmed-up measurements. The user's first run is often
  a cold-load reading ~10ms slow and will mislead you. Ask explicitly for the warmed-up number.
- Before claiming a change helped/didn't, get the real number; recalibrate framing if cold data misled.
- Don't add changes that don't actually help (e.g. cargo-cult `reserve()`); call it out and skip it.
- Offer the next step as an experiment with a clear hypothesis and a data table comparing before/after.

Build workflow: the user builds manually in Visual Studio 2026 (CLAUDE.md forbids me
from building). I deliver code edits + lightweight instrumentation; the user runs the
exe and pastes back console profile output. Design diagnostics to print to the Debug
console (std::cout/printf) since that's their feedback channel.

Language: the user writes in Chinese; respond in Chinese.

Tooling pattern that worked well: a temporary header-only profiler with RAII scope
timers + per-phase + counters, printing every N frames. The user engages well with
this and with before/after comparison tables.

Related: [pvz-perf-optimization](project_pvz_perf_optimization.md).
