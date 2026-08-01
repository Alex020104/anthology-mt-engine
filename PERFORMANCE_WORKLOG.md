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

## Source audit — 2026-07-31

Reference branches were fetched without merging them into Anthology:

- `upstream-themrdemonized/all-in-one-vs2022-wpo-mt`;
- `upstream-themrdemonized/pr-614` (13 commits from PR #614);
- `ixray-stcop/develop` and `ixray-stcop/default`.

### PR #614 loading work

- The PR parallelizes native level preparation while keeping Lua, spawning,
  renderer publication, and D3D11 immediate-context calls on their owner thread.
- Its own results show that hot-save loading changes very little (`-2.8%` in
  GAMMA and a small regression in vanilla). The PR explicitly identifies
  sequential Lua/ALife work as the remaining limit.
- Anthology v44 already contains adapted native preparation, R4 process-local
  level caching, sparse precache rendering, startup preparation, race fixes,
  and load-session diagnostics from this work.
- Therefore the next save-load work must target the measured Lua/ALife phases;
  duplicating renderer parallelism cannot turn the current 17.9 s load into 10 s.

### Monolith runtime MT audit

Already present in Anthology:

- scheduler split: real-time objects on the main thread and deferred objects in
  the secondary task group;
- idle-time parallel Lua GC;
- `CEnemyManager::useful` Lua-result cache with per-entity jitter;
- NPC sound callbacks in scheduled updates;
- sector/portal traversal work and multithreaded details.

Differences selected for compatibility review:

- current Monolith defaults `scheduler_batch_size` to 256 and allows a larger
  range, while Anthology currently defaults to 128 and caps it at 256;
- current Monolith has a simpler guarded details synchronization path; Anthology
  has additional load/unload protection that must be retained while removing
  redundant frame waits;
- IX-Ray particle loading reserves action storage before deserialization, a
  small and low-risk allocation optimization that is not present in Anthology.

Rejected or disabled-by-default upstream experiments:

- `mt_SchedulerRT`: upstream says it can cause problems; do not enable it;
- `mt_ui`: default-disabled and conflicts with the UI owner-thread rule;
- multithreaded HOM: previously disabled for border artifacts and later changed
  again; keep Anthology runtime HOM on the render thread;
- topological bone sorting: reverted upstream on 2026-05-24;
- ALife `xr_sparse_map`: repeatedly applied/reverted and finally reverted on
  2026-03-23; do not risk save compatibility with it.

### IX-Ray development branch

IX-Ray `develop` now has a substantially different renderer/RHI and task
architecture, so it cannot be merged or copied wholesale into Anomaly. The
safe route is the one used by Monolith: backport isolated algorithms and their
follow-up fixes. Current candidates are limited to compatible containers,
allocation reduction, wait/barrier fixes, and owner-thread-safe task usage.

## Candidate v45 — runtime barrier and load profiler

- Adapted the current Monolith details barrier: one guarded per-frame
  calculation replaces overlapping volatile flags and two `Sleep(0)` loops.
  Anthology's load/unload `Suspend`/`Publish` protection is retained.
- Updated the scheduler default to 256 and exposed the upstream-compatible
  upper range. RT objects remain on the main thread.
- Backported IX-Ray particle-action capacity reservation (source commit
  `04459cdaf9665857a2353351ac8bd928d12306a8`).
- Added owner-thread Lua load profiling. Each load session reports aggregate
  script self-time and the 20 slowest modules without changing callback order.
- Routine per-script debug spam is now opt-in with `-script_load_log`; errors,
  load-session diagnostics, and the rest of `-dbg` remain enabled.
- `DX11-AVX` build completed successfully. Candidate hashes:
  - EXE: `E9940BD3F601F6F6F368A361BE7DF51D39EA07344AB3068FC3AC4F5E8B4CDEC1`;
  - PDB: `9FDACAD822543CAE5743B50BED03BE55FFCA1E19AAD2B74555C93B5C0FAAC925`.
- Local `user.ltx` still contains `scheduler_batch_size 128`; installation of
  this candidate will back it up and set it to the upstream default of 256.

### v45 installation

- Full pre-install backup:
  `webcache/engine_v45_install_backup_20260731_204954` (17 files,
  663,827,486 bytes, including the previous `user.ltx`).
- Installed only `AnomalyDX11AVX.exe` and `AnomalyDX11AVX.pdb`; installed hashes
  match the candidate hashes above.
- Local scheduler setting changed from 128 to 256. The scheduler's existing
  per-frame time budget still limits work, so this raises capacity without
  moving real-time objects off the main thread.
- The game executable was not launched by the integration process.

## Candidate v46 - measured Lua and particle bottlenecks

### Current save-load evidence

- The latest stable v45 log completes without a crash, but the engine-ready
  session is still `52,865 ms`. The user's visible save-load interval is about
  `22-23 s`; the engine session also includes menu/startup work outside that
  visible interval.
- Native level preparation is already parallel and takes `3,261 ms`; resource
  wait is `147 ms`. The two texture loader reports are only `146 ms` and
  `42 ms`, so texture I/O is not the current save-load limit.
- Lua startup takes `9,279 ms` (`7,886 ms` measured script self-time). The
  largest modules are `aol_anim_transitions` at `3,317.85 ms` and
  `perk_based_artefacts` at `1,051.39 ms`.
- Final precache is dominated by game updates: `FrameMove=27,695.32 ms`, while
  level rendering is `276.47 ms`, render-sequence callbacks are `363.31 ms`,
  and secondary-worker waiting is `1,307.87 ms`. More D3D11 rendering threads
  cannot remove the measured serial Lua/object update cost.

### Adapted changes

- Optimized `ini_file:section_for_each` by directly invoking its bound Lua
  callback, avoiding a luabind proxy/converter allocation for every merged INI
  section while preserving iteration order and early-exit behavior.
- Added `ini_file:section_for_each_suffix`. The active R.A.K.
  `aol_anim_transitions.script` uses it for `_hud` sections with a fallback to
  the original API. This prevents thousands of callbacks for sections the
  module immediately discarded. The reproducible local patch is stored in
  `modpack-patches/aol_anim_transitions.script.patch`.
- Adapted the current Monolith/IX-Ray particle fix: strict B2F particle visuals
  are no longer inserted into the generic sorted queue in addition to their
  dedicated particle path. This removes duplicate CPU submission and GPU work
  in particle-heavy scenes without changing non-particle sorting or PiP/UI.
- Added `objects/hud/script` timing to the precache performance report. It is
  diagnostics only and will identify which part of `FrameMove` owns the next
  measured load bottleneck.
- Existing owner-safe texture preparation and render-preparation workers remain
  enabled. D3D11 immediate-context publication stays on the render thread;
  moving it blindly would reintroduce the UI/PiP/resource races seen in earlier
  candidates.

### Validation

- `DX11-AVX` builds successfully with the changes above.
- Candidate hashes:
  - EXE: `331CCB0F7A03A420F57B4A07524773D84A15475939533DE7BC3C0302C0B0C0C2`;
  - PDB: `F984326C8BE7E31FABAD173E1ABF3D957EB374C1D2745563FEA352953256EFB7`.
- The active R.A.K. script was backed up before installation under
  `webcache/modpack_v46_patch_backup_20260731_213517`.
- The game executable was not launched during validation.

### v46 installation

- Full pre-install engine/config backup:
  `webcache/engine_v46_install_backup_20260731_213743` (17 files,
  663,901,214 bytes). It contains the complete previous `bin` and `user.ltx`.
- Installed only `AnomalyDX11AVX.exe` and `AnomalyDX11AVX.pdb`; installed hashes
  match the candidate hashes above. Other renderer executables and DLLs were
  not changed.
- Enabled the existing owner-thread Lua functor lookup cache with
  `lua_use_functor_cache 1`. The setting is backed up and remains a one-line
  runtime fallback; `lua_busy_hands_debug 1` is retained for object safety.
- Kept `mt_level_call`, `mt_task_manager`, and `mt_ui` disabled. The standard
  engine MT mask, `r2_mt`, parallel Lua GC, and scheduler batch 256 remain
  enabled.
- Installed active R.A.K. script hash:
  `AE0C06BC2DA335AE3022039D0C9D66A59A5346E9117113CC25CC01D91D1825D4`.
- Installed `user.ltx` hash:
  `C0B08776F2E0872580B922E07EBEAFC09F3C085BCDDAACDED1E382C0D944369C`.
- The game was not launched; runtime/load/FPS validation is left to the user.

## Candidate v47 - guarded startup VFS and scheduler attribution

### New upstream evidence

- The `mt-load-test` release tag resolves to Monolith commit
  `9a83881851d0bff288dd13f8c93688620ef5da56`, the same final source revision
  already used for the parallel-level-load review. The foreign test binary was
  not copied into Anthology; its source changes were compared and adapted.
- The latest v46 log shows `Init FileSystem 14.115475 sec` and no
  `[STARTUP/VFS]` diagnostics. The bulk initial catalog, duplicate-alias scan
  elimination, archive indexing, and process-local loose-file cache existed in
  Anthology source but their activation had been removed by the earlier broad
  UI/runtime-safety rollback.
- The same log shows save-load precache dominated by `FrameMove` (29.7-38.6 s),
  while level rendering is only 20-272 ms. The remaining main-thread scheduler
  cost was not separately measured.
- A later v46 run on 2026-08-01 confirms the pattern at larger scale:
  `FrameMove=39,294.84 ms`, client spawn `34,397 ms`, native level `3,461 ms`,
  resource wait `3 ms`, and level rendering `596.24 ms`. Lua module self-time
  is `10,681.76 ms`, including `aol_anim_transitions=3,973.31 ms`.
- That run used unsafe persistent runtime values: `mt_ui=1`,
  `mt_task_manager=1`, `mt_level_call=1`, and
  `scheduler_batch_size=52736`. Upstream keeps UI and TaskManager MT disabled
  and uses a large scheduler batch only temporarily during initial actor load.

### Adapted changes

- Restored the upstream bulk VFS startup path without reverting any Anthology
  UI, XML, texture, PiP, renderer, or save compatibility changes.
- Recursive aliases already covered by an earlier recursive root are no longer
  scanned repeatedly during the initial catalog build. File publication stays
  ordered, preserving `fsgame.ltx`/MO2 override precedence.
- Enabled the existing process-local loose startup cache only in the fast path.
  `-no_startup_parallel` retains the previous exact serial scan behavior and
  disables this cache, providing a one-flag runtime fallback.
- Added main-thread scheduler attribution to the existing load-session
  diagnostics. RT object ownership remains on the main thread and scheduler
  ordering is unchanged.
- Adapted upstream scheduler flush inside the engine: deferred work may use the
  maximum batch only while the 60-frame loading precache is active, then the
  configured gameplay batch resumes automatically. The Lua
  `level.scheduler_flush` API and its modpack script were not copied.
- Removed the per-light CPU spin waiting for an unfinished GPU occlusion query.
  A pending result is treated conservatively as visible and retried next frame,
  preventing up to 0.5 ms of CPU waiting per queried light without hiding it.
- Portal traversal rejects only bit-identical repeated frustums and fixes the
  missing parentheses that could take the debug portal path even with
  `rs_render_portals` disabled.
- Adapted the current projected-size detail calculation and avoids repeated
  flag/container lookups in the inner grass visibility loop.
- Reviewed the July `codex/gpu-driven-visibility` experiments. Aggressive HOM,
  shadow hierarchy, and visibility changes were excluded because they can
  change image correctness and have not been stabilized in the main MT branch.
- No modpack Lua scripts, UI/XML files, PiP code, or save data were modified.

### Validation

- `DX11-AVX` MT build completed successfully. Existing project warnings remain;
  the v47 changes add no build errors.
- Candidate hashes:
  - EXE: `EE4C5C5FFBFAD9D1B7072E333B8E2F6ADD90557F466787377FDB623C6EA9487F`;
  - PDB: `498273748C52210A5C627BE8309810CF42326A21269513F418E6446200BEE51A`.
- The game was not launched during validation.
