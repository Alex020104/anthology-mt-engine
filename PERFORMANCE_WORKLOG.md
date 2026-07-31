# Anthology performance worklog

Branch: `anthology-monolith-ixray-performance`

This branch adapts performance and loading work from xray-monolith, PR #614,
and IX-Ray to the Anthology engine. Changes are reviewed and integrated by
subsystem; upstream branches are not merged wholesale.

## Integration rules

- Preserve Anthology UI, PiP, renderer behavior, scripts, and save compatibility.
- Keep render-owned and Lua-owned state on their owner threads.
- Separate loading, runtime threading, and diagnostics into reviewable commits.
- Build and validate DX11-AVX before installing a candidate.
- Keep full local `bin` backups; do not commit binaries or local backups.
- Record the source, adaptation, risks, validation, and user test result here.

## Baseline — 2026-07-31

- Base commit: `4d696f47` (`fix: remove incompatible Lua bytecode cache`).
- Installed v44 EXE SHA-256:
  `33B178321AFEDD11B5348BD480FA1E98491CEBF1B99938098C5F05340660F3BC`.
- Stable fixes retained:
  - parallel native level preparation and process-local level cache;
  - sparse precache rendering with all 60 logical callbacks preserved;
  - startup VFS/source preparation;
  - owner-thread protection for UI, PiP, dynamic textures, and level transitions;
  - detailed load-session diagnostics.
- Latest measured save load: `17.947 s` for 43,137 objects.
- Current dominant phases: Lua start callback about `8.8–9.0 s`; save/ALife
  restore about `5.9–6.3 s`.
- Rejected experiment: Lua bytecode caching. It saved only about `0.16 s` and
  broke case-sensitive addon namespaces, so it was fully removed.

## Research queue

- `themrdemonized/xray-monolith`: current and May-era runtime threading/frame pacing.
- `themrdemonized/xray-monolith#614`: loading implementation and follow-up fixes.
- IX-Ray development branch: transferable job-system, scheduler, render, and AI work.

