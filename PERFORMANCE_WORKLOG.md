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

### v47 installation

- Full pre-install engine/config backup:
  `webcache/engine_v47_install_backup_20260801_085019` (17 files,
  663,953,950 bytes). It contains the complete previous `bin` and `user.ltx`.
- Installed only `AnomalyDX11AVX.exe` and `AnomalyDX11AVX.pdb`; installed hashes
  match the v47 candidate hashes above. Other renderer executables and DLLs
  were not changed.
- Restored the safe runtime profile in `user.ltx`: `mt_level_call=0`,
  `mt_task_manager=0`, `mt_ui=0`, and `scheduler_batch_size=256`.
  `mt_scheduler=1`, `mt_calc_bones=1`, `r2_mt=on`, and the Lua functor cache
  remain enabled. The automatic large load batch is controlled by the engine
  and cannot leak into normal gameplay.
- Installed `user.ltx` hash:
  `F6753BA172FCF8A65DA6B37821E08902E630D950B0B099BFC8F5136DD3049B61`.
- The integration process did not launch the game executable.

## v47 runtime review and Ryzen 7 3700X balanced profile

### Fresh v47 evidence

- The 2026-08-01 user test is running the installed v47 executable. The bulk
  VFS path is active with seven level workers on the 8-core/16-thread CPU.
- `Init FileSystem` is `11.997974 s`, versus `18.403616 s` in the preceding
  v46 comparison log.
- Menu-save engine-ready time is `61,517 ms`, versus `76,614 ms` in the
  preceding v46 comparison. The measured v47 phases are server/Lua
  `15,922 ms`, native level `3,412 ms`, client spawn `26,209 ms`, and final
  precache `39,432 ms`.
- The remaining precache bottleneck is still serial world update work:
  `FrameMove=30,866.08 ms`, `scheduler main=3,739.58 ms`, and secondary wait
  `5,510.28 ms`; renderer work is only `516.73 ms`. Graphics settings cannot
  by themselves turn this save into a 10-second load.
- The load uses `native=parallel`, save/spawn prefetch, level cache, and sparse
  precache. This confirms that worker creation is not the missing switch;
  save scripts, client spawn, and main-thread simulation remain the dominant
  path.

### Installed settings profile

- Backed up the complete pre-change `user.ltx` and MO2 MCM store to
  `webcache/performance_profile_backup_20260801_091503` before editing.
- Synchronized the main game menu and MCM values: detail radius `150 -> 110`,
  density `0.78 -> 0.65`, geometry LOD `1.5 -> 1.2`, visibility `0.9 -> 0.8`,
  skeleton update `32 -> 24`, sun and sunshafts High -> Medium, NPC dynamic
  torches off, actor shadow off, and wallmark lifetime `250 -> 150`.
- Replaced the stale MCM scheduler value `52736` with the safe gameplay value
  `256`. Engine-managed temporary load batching remains enabled.
- Applied an SSS/SSFX balanced profile: AO High -> Medium, IL Very High ->
  Medium, interactive entities `8 -> 5` and distance `2000 -> 1200`, POM High
  -> Medium without refine, shadow maps `1536/4096 -> 512/2048`, forced
  volumetric point lights off, volumetric quality Very High -> Medium, grass
  shadow Ultra -> Medium, SSR Ultra -> High at 0.7 scale, SSS Very High ->
  Medium, and terrain POM High -> Medium.
- Reduced Spatial Audio ray cost through its MCM data: player/NPC bounce counts
  `3/2 -> 2/1`, ray work per frame `2 -> 1`, and NPC interval
  `1250 -> 2000 ms`.
- Preserved 1920x1080 resolution, full texture quality, 16x anisotropy, TAA,
  wet surfaces, volumetric smoke/lights, water effects, PiP/3D scope values,
  and the safe engine MT mask. No Lua, XML, UI, shader, weapon, or save files
  were modified.
- Final hashes: `user.ltx`
  `6B652C4B34E00ABFD583BB90BC7620EF3AA026B22C83FC4F300C48A5A3535AA3`;
  `axr_options.ltx`
  `9FD6A4BE6DFC8FDA888396683B660AF57D7531EFFB44264A8625B1E376C9976F`.
- The game was not launched. The user will compare the same save and camera
  position and provide the next log/FPS capture.

## v47 settings-layer correction and second runtime review

### Why the base config appeared unchanged

- The writable MCM store used by the MO2 launch is
  `SYS_A.N.T.H.O.L.O.G.Y_mo2_CBT/overwrite/gamedata/configs/axr_options.ltx`.
  The latest game session incremented its session metadata and preserved the
  installed performance values, confirming that it is the active VFS layer.
- The physical `Anomaly-1.5.3-Anthology 2.1/gamedata/configs/axr_options.ltx`
  and its legacy root duplicate had remained stale. This made the settings
  change invisible when inspecting the physical game directory and could also
  produce different settings when launching outside the configured MO2 path.
- Backed up all three `axr_options.ltx` layers and `user.ltx` to
  `webcache/axr_sync_backup_20260801_093345` before correcting the mismatch.
- Synchronized the selected core, Modded Exes, SSS/SSFX, and Spatial Audio
  performance keys into both physical CP1251 files while preserving their
  encoding and CRLF line endings. The physical copies are now byte-identical,
  SHA-256
  `A5DB4EBAFAABEA885D165EAE220CF5FFD06EEDE681A828C61629CD22713BC0AE`.
- The active MO2 store and `user.ltx` already contained the target values and
  were not overwritten during this correction. PiP/3D scope values and the
  safe MT mask remain unchanged. No Lua, XML, UI, shader, save, or weapon file
  was modified, and the game was not launched.

### Latest load evidence

- The next user test still reports about 22 seconds of visible save loading.
  The fresh log confirms seven level workers on the 8-core/16-thread CPU and
  `native=parallel`, save/spawn prefetch, level cache, and sparse precache.
- The measured phases are server/Lua `15,572 ms`, native level preparation
  `3,482 ms`, client spawn `26,898 ms`, and final precache `41,049 ms`.
  Precache attribution is `FrameMove=31,967 ms`, main-thread scheduler
  `4,106 ms`, secondary-thread wait `6,304 ms`, and rendering only
  `491.85 ms`.
- Therefore neither missing worker creation nor SSS rendering cost explains
  the 22-second load. The remaining load-time target is serial FrameMove,
  Lua/client spawn, scheduler, and synchronization work. Further graphics/MCM
  reductions may improve gameplay FPS but cannot plausibly reduce this save
  to 10 seconds on their own.

## v48 owner-safe spawn and sparse-precache worker reduction

### Runtime target

- The latest v47 log processes 4,040 client spawns and 1,792 client events.
  Client spawn accounts for `26,898 ms` of the load session and overlaps the
  `31,967 ms` aggregate FrameMove cost.
- Sparse precache renders the world on 12 of 58 measured frames, but the
  previous code still scheduled HOM/detail preparation and visible-skeleton
  bone calculation on all 58 frames. Their results on the 46 skipped frames
  were overwritten before a world frame could consume them and contributed to
  the `6,304 ms` secondary-thread wait.
- Current Monolith MT and IX-Ray development implementations were reviewed
  again. Object creation, `net_Spawn`, Lua callbacks, real-time scheduler work,
  and renderer publication remain owner-thread operations in this adaptation.

### Adapted changes

- World-render-only secondary jobs (`seqParallelRender` HOM/detail work and
  `CalculateBonesThread`) now follow the same sparse-precache decision as the
  world renderer. They still run on every displayed precache world frame and
  every normal gameplay frame.
- All 60 logical precache frames, FrameMove, object/HUD/script updates,
  particles, Lua-visible callbacks, scheduler ordering, loading-screen draws,
  and the final world render remain intact.
- `ProcessGameEvents` now reuses one 16 KiB `NET_Packet` across its serial
  owner-thread loop instead of allocating a three-packet helper for every
  event. A durable packet copy is still made when an event is handed to the
  asynchronous prefetch queue, and the rare move-player response allocates its
  packet only when used.
- `ProcessSpawnEvents` likewise reuses one owner-thread packet across its
  batch. Spawn order, packet contents, ALife validation, model publication,
  `cl_Process_Spawn`, and Lua callback order are unchanged.
- No Lua, XML, UI, PiP, shader, config, save, weapon, or renderer-content file
  was modified.

### Build and installation

- `DX11-AVX` Release build completed successfully. The modified translation
  units compile and link; only pre-existing project warnings remain.
- Candidate and installed hashes match:
  - EXE: `DBDD801B245C954A78D6AF75192B0515EE40C5A071B4D95C7FCFD7BB244C4F27`;
  - PDB: `3B7706F981A9D5CE341B5D227D5D1F05F3029CA40B25AAE427CF345389F24812`.
- The previous v47 EXE/PDB are backed up in
  `webcache/engine_v48_install_backup_20260801_100621` with hashes
  `EE4C5C5FFBFAD9D1B7072E333B8E2F6ADD90557F466787377FDB623C6EA9487F`
  and `498273748C52210A5C627BE8309810CF42326A21269513F418E6446200BEE51A`.
- The game was not launched during validation. The next same-save test should
  compare visible load time and the log's `secondary wait` and `client spawn`
  fields; no specific improvement is claimed before that measurement.

## v48 measured result and v49 client-spawn attribution

### v48 result

- The user tested the installed v48 executable on the same save. It was active
  with hash `DBDD801B245C954A78D6AF75192B0515EE40C5A071B4D95C7FCFD7BB244C4F27`.
- v48 did not produce a useful total-load improvement. Engine-ready time
  regressed from `62,658 ms` to `76,496 ms`; client spawn increased from
  `26,898 ms` to `35,359 ms`; FrameMove increased from `31,967 ms` to
  `40,692 ms`.
- The intended render-worker reduction is visible but immaterial:
  secondary wait decreased from `6,304 ms` to `5,524 ms` (about `780 ms`).
  This is not sufficient and is not treated as a successful optimization.
- The session still creates 4,036 client objects and processes 1,790 events.
  Native level preparation remains parallel at `3,687 ms`; the unresolved
  target is the serial client object construction and callback path.

### v49 attribution and safe lookup removal

- Added owner-thread timing around each client spawn with aggregate categories:
  server-entity decode, client object construction/`Load`, `net_Spawn`, post
  spawn callbacks, `Game::OnSpawn`, and residual work.
- The log now prints the aggregate `[client-spawn/profile]` line and the 15
  sections with the highest cumulative spawn time. This provides the data
  needed to choose a concrete preparation stage for the next worker adaptation
  instead of changing Lua/object ownership speculatively.
- Removed one duplicate `system.ltx` class lookup per client object. The client
  object pool now accepts the `CLASS_ID` already resolved and validated by the
  corresponding server entity. Constructors, `Load`, spawn order, packet data,
  callbacks, and owner thread are unchanged.
- `DX11-AVX` Release build completed successfully. Installed hashes:
  - EXE: `5556A364C08005EAD12FF2B6D15646072BF333E7B32A546EE0C4F0B3FE48B7C4`;
  - PDB: `8F42AE3DEDEF400D6D068649EEEFA51F863ABC99FB206F4EE5B056479BDF1542`.
- The previous v48 EXE/PDB are backed up in
  `webcache/engine_v49_spawn_profile_backup_20260801_110333`.
- No Lua, config, UI/XML, PiP, shader, save, or weapon file was modified. The
  game was not launched during build or installation.

## v49 measurement and v50 safe spawn/frame parallelism

### Measured target

- The first profiled v49 load processed 4,038 client spawns. Aggregate client
  work was `33,540 ms`: server-entity decode `4,936 ms`, client object
  create/load `10,024 ms`, `net_Spawn` `17,850 ms`, and callbacks `725 ms`.
  One actor spawn accounted for `13,985 ms` of the aggregate total.
- The same-level quickload measured server/Lua `18,859 ms`, native level
  preparation `1,554 ms`, client spawn `13,355 ms`, and precache wall time
  `45,341 ms`. Native level preparation and texture reuse are already fast;
  the remaining target is object/Lua work and the per-frame CPU path.
- The active configuration already enables `r2_mt`, `mt_scheduler`,
  `mt_calc_bones`, `r__optimize_calculate_bones`, and the enemy-manager useful
  cache. `r__clear_resources_on_unload` is off, so quickloads retain reusable
  resources. PiP/3D scopes remain enabled.
- Hardware inspection found a Ryzen 7 3700X (8C/16T), RTX 5070 12 GB, and
  32 GB DDR4. The DIMMs are rated for 3200 MT/s but are currently configured
  at 2400 MT/s; Windows uses the Balanced power plan. The supplied gameplay
  capture showed a 94 C CPU and about 35% GPU use. The selected 1080p graphics
  profile is therefore not the principal FPS limiter.

### Adapted implementation

- Added parallel server-entity packet decode for large load-session spawn
  batches. Packet preparation runs on workers, while renderer resource
  publication, client object construction/load, `net_Spawn`, ALife checks,
  and every Lua-visible callback remain on the owner thread and retain the
  original event order.
- The worker allow-list is deliberately conservative: script-factory classes,
  actor, every `AI_*` class, and sections with `custom_data` use the original
  owner-thread path. This avoids the shared Lua VM, live ALife registry, and
  process-wide RNG. The feature is exposed as `mt_load_spawn_decode` and is on
  by default.
- Added `[client-spawn/decode]` wall-time/count diagnostics and reset spawn
  profiling after each completed session, so a same-level quickload now emits
  a fresh profile. Top sections include decode, create/load, `net_Spawn`, and
  callback attribution instead of only a total.
- Split independent visible skeleton `CalculateBones` calls through the
  existing parallel scheduler when at least eight visuals need work. Existing
  frustum/distance rejection and each visual's own synchronization remain in
  place.
- Split expensive detail-blade transform/cull preparation by visible cache
  slot when at least 32 slots are active. Frustum/HOM tests, random refresh
  scheduling, and publication into renderer-visible vectors stay deterministic
  and single-writer. No grass shaders, SSS parameters, or detail content were
  changed.
- No Lua, XML/UI, PiP, shader, save, weapon, or modpack config file was
  modified by v50.

### Build candidate

- `DX11-AVX` Release compiled and linked successfully. The only linker warning
  is the pre-existing duplicate `lj_vm.obj` entry in the LuaJIT project.
- Candidate hashes:
  - EXE: `8159871239F5CEF64482CD81530B304B3FFEC2D5DB02BB29EB15BCA41E06CE3B`;
  - PDB: `4C4D7A1A12D3E249AD983230516B24AEF29E7F3C30F1D9BD00BE7AE2FFD33463`.
- The game was not launched. Load-time and FPS gains remain to be measured on
  the user's same save and same gameplay position.

### Installation

- v50 EXE/PDB were installed to `Anomaly-1.5.3-Anthology 2.1/bin`; installed
  hashes match the build candidate exactly.
- The preceding v49 pair was backed up to
  `webcache/engine_v50_parallel_spawn_frame_backup_20260801_115207`:
  - EXE: `5556A364C08005EAD12FF2B6D15646072BF333E7B32A546EE0C4F0B3FE48B7C4`;
  - PDB: `8F42AE3DEDEF400D6D068649EEEFA51F863ABC99FB206F4EE5B056479BDF1542`.
- No Anomaly/XRay process was running during replacement, and the game was not
  launched afterward.

## 2026-08-01 - v51 measured Lua-load and frame-worker correction

### Evidence from the user's v50 run

- v50 did not produce a repeatable improvement: the user's quickload remained
  in the 16-23 second range and gameplay FPS was unchanged.
- The fresh same-level quickload profile attributes only `55.22 ms` to spawn
  decode and `158.54 ms` to entity creation/load. The experimental v50 decode
  batch never activated because the real event batches remained below its safe
  threshold, so this was not the remaining load bottleneck.
- The same profile attributes `10811.88 ms` of the actor's `10813.37 ms`
  client-spawn time to serial `net_Spawn` work. The log also shows two explicit
  pairs of full Lua collections during the loading callbacks, with multi-second
  gaps and large run-to-run variance. Native level preparation remained only
  `1687 ms`.
- Current Monolith intentionally executes the visible-skeleton loop serially
  inside its already asynchronous frame task. v50's nested PPL skeleton and
  detail loops therefore diverged from upstream and added dispatch/barrier
  overhead without a measured FPS gain.

### v51 adaptation

- Wrapped the standard Lua `collectgarbage` entry point at VM initialization.
  Only a default/full `collect` requested while an engine load session is
  active is deferred; `count`, `step`, stop/restart, pause/step multiplier,
  arguments, return values, and error behavior still forward to LuaJIT. No
  modpack Lua file was changed.
- Deferred full collections are reported as `[load-session/lua-gc]`. After the
  player receives control, the existing incremental collector continues the
  cleanup instead of imposing two stop-the-world collections on the load path.
  The behavior is reversible with `load_defer_full_lua_gc 0`.
- Added a `250 us` per-frame budget to the parallel incremental Lua collector
  (`lua_parallel_gc_budget_us`). It still performs at least one step, but can no
  longer occupy a PPL worker for the entire render and inflate the final frame
  barrier on a large Lua heap.
- Made the cross-thread renderer-active flag atomic and removed v50's nested
  PPL fan-out for skeletons and detail slots. Both subsystems still overlap the
  main renderer through their existing secondary task.
- Disabled `mt_load_spawn_decode` by default because the measured decode cost
  is negligible and the safe path did not activate. The diagnostic path remains
  available for controlled tests.
- Added low-overhead 300-frame telemetry under `mt_frame_profile`. Log records
  average frame move, render, final worker wait, pre/post-render work, bones,
  game worker, Lua GC, and maximum frame/wait time. This is enabled for the next
  user test so subsequent FPS work can target the measured subsystem.
- No UI/XML, PiP, shader, save format, weapon data, MCM setting, or modpack Lua
  script was modified.

### Build candidate

- `DX11-AVX` Release compiled and linked successfully. The only linker warning
  is the pre-existing duplicate `lj_vm.obj` entry in the LuaJIT project.
- Candidate hashes:
  - EXE: `1AFDFC1499EA8C04352212B251329FA2635DD7AE52E72D3BF666409305A10A0A`;
  - PDB: `17F9844463843615F03CD2E5DA2D900B2E4BF91E03261B19837647AFF0427B44`.
- The game was not launched. The same save can be used; no new game is needed.

### Installation

- v51 EXE/PDB were installed to `Anomaly-1.5.3-Anthology 2.1/bin`; installed
  hashes match the candidate exactly.
- The preceding v50 EXE/PDB and the pre-v51 `user.ltx` were backed up to
  `webcache/engine_v51_gc_frame_backup_20260801_125006`:
  - EXE: `8159871239F5CEF64482CD81530B304B3FFEC2D5DB02BB29EB15BCA41E06CE3B`;
  - PDB: `4C4D7A1A12D3E249AD983230516B24AEF29E7F3C30F1D9BD00BE7AE2FFD33463`;
  - `user.ltx`: `804F2EAEE3E1735A7FD3349F99A0E39776092B3AE24570991FCA8FF749077358`.
- The active engine-only settings are `load_defer_full_lua_gc 1`,
  `lua_parallel_gc_budget_us 250`, `mt_frame_profile 1`, and
  `mt_load_spawn_decode 0`. No graphics/MCM/modpack settings were changed.
- No Anomaly process was running during replacement, and the game was not
  launched afterward.

## 2026-08-01 - v52 MT stalker movement crash guard

### Crash evidence

- The fresh minidump and PDB-resolved stack end in
  `stalker_movement_manager_base::setup_movement_params` at the destination
  vertex replacement path. The caller chain is `GameThread` -> deferred
  scheduler -> `CAI_Stalker::Think` -> stalker movement update.
- The active runtime keeps `mt_scheduler 1`; the failure is therefore an
  engine-side hard crash in the parallel game worker, not a Lua error and not
  a Tactic Compass callback failure.
- Current Monolith already addresses this exact source line in upstream commit
  `efda92014`. The Anthology branch was missing its vertex guards and also
  retained the older `on_restrictions_change` pointer-negation typo fixed by
  upstream commit `5d187e8a6`.

### Adapted fix

- Validate every level-graph vertex before dereferencing it or publishing it
  as the NPC destination.
- When a destination is invalid, first recover it from the NPC's current valid
  vertex. If recovery is impossible, force that NPC to stand for the update
  instead of calling `vertex_position` with an invalid id.
- Validate the results returned by `accessible_nearest` in both movement setup
  and nearest-position recovery, and reject invalid ids in the common
  `CMovementManager::set_level_dest_vertex` entry point.
- Correct `accessible(!m_current.desired_position())` to dereference the actual
  desired position. This lets restriction changes rebuild the path instead of
  leaving a stale destination behind.
- The MT scheduler remains enabled and `scheduler_batch_size` remains 256. No
  Lua, UI/XML, PiP, shader, save, weapon, graphics, or modpack configuration
  file was changed.

### Build and installation

- `DX11-AVX` Release compiled and linked successfully. Only the pre-existing
  LuaJIT duplicate-object and template warnings remain.
- Candidate and installed hashes match:
  - EXE: `20AA24BC47832051BDB174298B1E38E5EB516441428149209C2DD83920BF97E0`;
  - PDB: `5E90AC161239482556B0BBF1993601F06928DD2A67219C476665019836701EB1`.
- The preceding v51 EXE/PDB were backed up to
  `webcache/engine_v52_ai_movement_guard_backup_20260801_201705`:
  - EXE: `1AFDFC1499EA8C04352212B251329FA2635DD7AE52E72D3BF666409305A10A0A`;
  - PDB: `17F9844463843615F03CD2E5DA2D900B2E4BF91E03261B19837647AFF0427B44`.
- No Anomaly process was running during replacement, and the game was not
  launched afterward. The same save remains valid for the regression test.

## 2026-08-01 - addon optimization batch 1

### Measured targets

- The fresh load profile attributes `3,530.47 ms` of Lua self-time to
  `aol_anim_transitions` during `start_game_callback`. Both enabled MO2 copies
  were still calling the unfiltered `section_for_each`, so the suffix-filtered
  engine API added earlier was present in v52 but not actually used by the
  active modpack.
- Interaction Dot Marks walked every managed HUD marker at a nominal 15 ms
  cadence. Its target-object queries were also performed before the cadence
  guard, which made them run once per rendered frame.
- `arrival_environmental_particles` performed five upward cover rays and a
  full particle-state pass on every `actor_on_update` outdoors.
- Ledge Grabbing was configured with `throttleCheck=0`, alternate detection
  enabled, and 15 ray steps, so its multi-ray climb search could also execute
  every rendered frame.

### Adapted changes

- Installed the guarded `_hud` suffix iterator in both enabled
  `aol_anim_transitions.script` owners. Other executables retain the original
  iterator fallback.
- Interaction Dot Marks now refreshes managed marker placement and hover at
  30 Hz. The cadence guard runs before target-object queries. Marker contents,
  interactions, XML, scaling, and PiP behavior are unchanged.
- Both possible MO2 providers of `arrival_environmental_particles.script` now
  check weather, cover and particle placement every 100 ms. Particle systems
  continue simulating between checks; the change removes roughly 83% of the
  script's outdoor cover rays at 60 FPS.
- Ledge Grabbing now uses 10 ray steps and a 30 ms scan throttle in the live
  MCM override and both mirrored `axr_options.ltx` files. Climbing remains
  enabled, including alternate and player-width detection.
- Reproducible patches are stored in `modpack-patches`. No weapon, save, UI
  XML, renderer, SSS, PiP, or engine binary file was changed in this batch.

### Installed hashes

- Kristiano AOL: `07950F0629ECE4907B487FCD498357142AF72685BA93233DED5CAEA1B8F5B428`
- R.A.K AOL: `3CCF4DBC017BB180FD1BF0CCA06924F19FD65AF0ABC877D3024EE8FACA5806F5`
- Dot Marks HUD manager: `4FF3D18DD12125FA93E6BCC19F6D319C81D2948A5E9B579A9AE7A969698FA048`
- Arrival particle provider: `864031F61B810B2DC0EC4AFD4EE45590C95F1DA027D8458001C98D11398B9D41`
- Seeds and Leaves particle provider: `C7A6BB73808FFD33344CD6328CDB400631397E0BF7615E630098BF885A96E61E`
- Active MO2 MCM override: `AF66F9EAFCF4465EFD3A106B56C17B37408653776ACB2B941D0E917114B6E0A6`

### Test expectation

- The existing save is valid; no new game is required.
- The next fresh log should show a large reduction in
  `[load-session/lua-profile] aol_anim_transitions` and lower outdoor script
  time. The remaining render-bound 11-15 ms component is outside this Lua-only
  batch and should be evaluated separately after the regression test.

## 2026-08-12 - addon batch rollback and v53 load-GC barrier correction

### Regression evidence and rollback

- The addon batch above did not improve the user's measured frame pacing and
  was already restored in the live MO2 installation. The active AOL, Arrival,
  Seeds and Leaves, Dot Marks, and Ledge Grabbing files/settings again match
  their pre-batch versions; no modpack Lua/config file is changed by v53.
- Removed the four stale patch artifacts from `modpack-patches` so this branch
  no longer represents the rejected addon experiment as an installed change.
- The fresh v52 load profile instead identifies an engine-side barrier:
  precache spent `11542.83 ms` waiting for secondary tasks. Frame telemetry
  also recorded recurring 40-68 ms worker waits and isolated waits up to
  303.78 ms, including when render and game-worker time were negligible.

### Root cause

- `load_defer_full_lua_gc 1` suppressed four explicit full collections during
  loading, but v52 never performed a replacement collection before returning
  control. The large garbage backlog was therefore collected incrementally in
  gameplay.
- Incremental Lua GC ran inside `Device.secondary_tasks`, which the render
  thread must join at the end of every frame. The outer 250 us budget cannot
  interrupt a single LuaJIT `LUA_GCSTEP` after it enters the collector's atomic
  phase, so a nominally parallel task became a periodic stop-the-world frame
  barrier.
- Monolith briefly moved this work to a persistent GC thread in upstream
  commit `a96e3e4702`, then reverted it immediately in `8444a0c5b1`. That
  implementation still spin-waited for GC and risked overlapping later Lua VM
  use, so it was not copied into Anthology.

### v53 adaptation

- While a load session is actively deferring full collections, incremental GC
  is no longer submitted to the per-frame secondary task group. This removes
  LuaJIT atomic phases from the loading precache barrier while preserving the
  existing parallel incremental collector during normal gameplay.
- All deferred full-collection requests are coalesced into one owner-thread
  `LUA_GCCOLLECT` immediately before the load session finishes. The garbage is
  reclaimed behind the loading screen instead of producing post-load stutters.
- Added a dedicated full-GC delegate with symmetric bind/clear handling during
  level load, reload and shutdown. The log now reports request count and the
  exact coalesced collection time as
  `[load-session/lua-gc] coalesced full collection`.
- UI/XML, PiP, shaders, saves, weapons, graphics settings and modpack scripts
  remain untouched. The current save remains compatible; no new game is
  required.

### Build and installation

- `engine-vs2022.sln`, configuration `DX11-AVX|x64`, compiled and linked
  successfully. Candidate and installed hashes match:
  - EXE: `512CB6D28190171C8A6366FC60F4B892E09827035676BDF545C0E1F984A4251D`;
  - PDB: `88848892E187102857A40F191A033DC252EAFEE7CFF045BCB16B19163657B609`.
- The preceding v52 pair was backed up to
  `webcache/engine_v53_load_gc_barrier_backup_20260812_010800` before v53 was
  installed to `Anomaly-1.5.3-Anthology 2.1/bin`.
- The active test settings remain `load_defer_full_lua_gc 1`,
  `lua_parallel_gc 1`, `lua_parallel_gc_budget_us 250`, and
  `mt_frame_profile 1`. No Anomaly/XRay process was running during replacement,
  and the game was not launched afterward.

### Test target

- Use the same save and measure from pressing Load until control is available.
  The next log should show one coalesced GC, sharply lower precache
  `secondary wait`, and lower post-control `max(total/wait)` values. The
  collection duration will reveal whether the remaining load target is GC or
  the separately measured actor `net_Spawn` path.

## 2026-08-12 - v54 quickload GC lifetime and animation-stall correction

### v53 result

- v53 did not meet the requested target. The measured menu-save session was
  `49.929 s` versus `51.225 s` in the preceding comparison. Precache secondary
  wait improved from `11.543 s` to `4.413 s`, and the coalesced full collection
  cost only `226.72 ms`, but client spawn and frame-move work absorbed most of
  the saved time.
- A later same-level quickload completed in `42.964 s`. Its log contained
  `[load-session/lua-gc] unable to run coalesced full collection: Lua VM
  unavailable`; after this point the parallel GC delegate was also absent.
- The quickload client profile attributed `7.615 s` of `8.228 s` total spawn
  work to the actor's serial `net_Spawn`. This is now split into named phases
  for the next measurement instead of guessing which owner-thread subsystem is
  responsible.
- One NPC/profile/visual combination threw inside the animation manager and
  emitted the same detailed error `1,556` times between loads. Each update paid
  for exception unwinding, animation resets, inventory logging and synchronous
  log output, matching the reported recurring stutters.

### v54 adaptation

- Do not clear Lua GC delegates when the load-game network message is received.
  Same-level quickload keeps the `CLevel` instance and does not call
  `CLevel::Load` again, so the old clear permanently disabled the collector.
  Actual level teardown still clears all delegates in `CLevel::net_Stop`, and a
  new level binds them normally.
- Latch a failed stalker animation update after its first detailed report. The
  bad NPC is no longer allowed to throw and log every scheduler update.
  `reinit()`/`reload()` clears the latch, so a rebuilt profile or changed visual
  can recover normally.
- Added `[actor-spawn/profile]` timing for inventory-owner, inherited object,
  physics, visual reload, final bones, map/statistics and unclassified actor
  spawn work. This instrumentation runs only during an active load session.
- Frame telemetry now defaults off. The installed test configuration also uses
  `mt_frame_profile 0`, `mt_load_spawn_decode 1`, `lua_use_functor_cache 1`, and
  `lua_busy_hands_debug 0`. Parallel decode remains restricted to native
  data-only entities; actor, AI, Lua-created and custom-data entities stay on
  the owner thread.
- `r__no_ram_textures` remains on because the 32 GiB system has insufficient
  evidence of safe headroom under this modpack. No graphics/MCM value, UI/XML,
  PiP, shader, save format, weapon data or modpack script was changed.

### Build and installation

- `engine-vs2022.sln`, configuration `DX11-AVX|x64`, compiled and linked
  successfully. Candidate and installed hashes match:
  - EXE: `42B012CDBB3C5D858F1427CD8316081628CE9E1BBB20080150CF1BFB87EDB763`;
  - PDB: `323317E32B9F35BFD3628915933F42CE60930E0092533D458359332811BF866B`.
- The preceding v53 pair and the exact v54 test `user.ltx` were copied to
  `webcache/engine_v54_quickload_stutter_backup_20260812_023947` before v54 was
  installed. No Anomaly/XRay process was running during replacement, and the
  game was not launched afterward.

### Test target

- Use the same save, then perform one same-level quickload. Both sessions must
  report a successful coalesced full collection; `Lua VM unavailable` must not
  recur. The broken Duty NPC error should appear at most once per NPC reload,
  not thousands of times. The new actor phase line determines the next load
  optimization without moving renderer, physics or Lua ownership unsafely.

## 2026-08-12 - v55 standalone addon load pass and dual DX11 build

### v54 measurement

- The fresh stable menu-save session reached engine-ready in `43.279 s`.
  Native level preparation took `2.247 s`, while server/Lua work took
  `10.546 s`, client spawn `18.880 s`, and final precache `27.786 s`.
- Client-spawn profiling attributed `4.798 s` to packet decode, `3.235 s` to
  entity creation/load and `10.479 s` to owner-thread `net_Spawn` work. The
  actor alone consumed `9.917 s`; its inherited actor/Lua path was `7.448 s`.
- Lua module startup took `5.524 s`. The two largest avoidable configuration
  scans were `aol_anim_transitions` (`2.568 s`) and
  `perk_based_artefacts` (`0.740 s`).
- The repeating animation exception storm fixed in v54 did not recur. Only one
  report remained for the bad `sim_default_duty_2` animation state, so this is
  no longer the source of a periodic gameplay stall.

### Standalone addon adaptations

- Created complete, independently removable addons under
  `D:/ANTHOLOGY_DEV/addons`; no source mod was edited in place:
  - `Anthology Performance - AOL Transitions`: replaces a scan of every system
    section with the existing `_hud` suffix index. Script SHA-256 changed from
    `5C0E6D89D7F53A4301744253655544398E6FE2D52C5F7997D430C3F61EFF7BA4`
    to `733F0995597BFBBA7C33A183192A07CBA293B30109786814F26CE72E64873386`.
  - `Anthology Performance - Perk Based Artefacts`: builds one immutable
    section index and derives all seven artefact sets from it instead of
    scanning `system.ltx` seven times. Script SHA-256 changed from
    `B9750E3606C8D3EBE8DD7C3ECBD3EF1EF89C0914E2ECF35A95D94A1D1E8E7D60`
    to `F6445835DD37BE6924983A01C8BDBF52CA8FA044661C32AF7F520110A69F9034`.
  - `Anthology Diagnostics - Actor Load Callbacks`: adds load-session-only
    timing around every `on_game_load` callback without changing callback
    order, arguments or error propagation. It reports total callback time and
    the twenty most expensive source locations. Script SHA-256 changed from
    `1B2C2049704A705EAE1DA34A86476D0B30D776EA94AE75E2D49EFF40055FEEA`
    to `C8965791FE5CB0CD5AF1140C7B4F121E68F81C516574CD457ACBDAB9223F0733`.
- All three scripts pass Lua 5.1 bytecode parsing. The addon directories are
  installed into MO2 as directory junctions and enabled at the top of the
  active `Anthology 2.1 HARD Сложный` profile, so edits stay on D while the
  game sees normal MO2 mods.
- Byte-exact originals and the pre-change active mod list are stored at
  `E:/ANTHOLOGY_BACKUPS/20260812_031500_addon_load_optimization`.

### Dual DX11 build and installation

- Both `DX11|x64` and `DX11-AVX|x64` configurations compiled and linked
  successfully from the same v54 engine source. Both executable/PDB pairs were
  installed to the game `bin` directory:
  - regular DX11 EXE:
    `C641D801BACAD1CA350B47A452DAACD67554620D226B524A841B761799174FFA`;
  - regular DX11 PDB:
    `14CBB766CB659FDE9D339D99BAE9C90E9933776858F5B5446F2399C34657CA03`;
  - DX11-AVX EXE:
    `BF36096AC2D1A405EBEF7EB3AACFA7C184EE24E5BB63772B3DB5023BBC2F2ED1`;
  - DX11-AVX PDB:
    `5B5903E59637AE4DBE3A3114ACADD70DBA3FAD34A71E871933BD69ABE9E6575B`.
- The replaced regular and AVX pairs are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260812_034100_engine_v55_dual_dx11/bin`.

### PiP inspection and next implementation plan

- The current PiP MCM quality selector writes
  `scope_lense_render_quality`, but no renderer code reads this variable. It is
  currently a no-op and cannot change image quality or performance.
- Current mouse scaling applies optical FOV scaling but exposes no PiP-specific
  user multiplier. PiP activation also has no engine-level policy for 2D NVG
  or thermal overlays.
- TAA jitter is disabled whenever PiP is active, rather than only for the
  viewport being rendered. This can make the lens excessively sharp/grainy
  and can disturb temporal stability in the main view even though the two
  viewports already have separate histories.
- The staged implementation and compatibility matrix are documented at
  `D:/ANTHOLOGY_DEV/addons/Anthology PiP Rework/PLAN.md`. This directory is a
  planning workspace only and is deliberately not enabled in MO2 yet.

### Test target

- Load the same save with the AVX binary first. The next log must contain
  `[load-session/lua-callbacks]`; its ranked callback list identifies the safe
  standalone addon targets inside the remaining `7.448 s` actor load path.
- Compare load-button-to-player-control time, not the earlier `Зона ждёт`
  message. No new game is required. Use regular DX11 only as a compatibility
  control; it contains the same engine logic without the AVX target.

## 2026-08-12 - v56 shadow rollback, AOL reparse fix and precache callback profile

### Fresh v55 load evidence

- The first menu-save load reached engine-ready in `47.820 s`; a same-level
  quickload reached engine-ready in `43.491 s`. This confirms that v55 did not
  yet deliver the requested wall-time reduction on this pack.
- The menu-save session spent `9.945 s` in server/Lua work, `2.308 s` in native
  level preparation, `20.720 s` in client spawn and `30.086 s` before final
  precache completion. The quickload spent `11.887 s`, `0.817 s`, `8.496 s`
  and `33.502 s` in the same phases.
- Precache frame movement dominated both cases: `23.206 s` for menu-save and
  `26.225 s` for quickload. The already instrumented main scheduler explained
  only `2.123 s` of quickload, so the former aggregate timing still hid the
  actual expensive `seqFrame` participant.
- The standalone PBA adaptation reduced its startup scan from the earlier
  `0.740 s` to `0.146/0.128 s`. AOL still consumed `2.449/2.132 s`, because its
  temporary `ini_file("system.ltx")` reparsed the full merged configuration
  before the suffix iterator could filter sections.

### Controlled SSS rollback

- The current profile had `ssfx_shadows=(768,1536,0)` and MCM shadow LOD minimum
  index `3`, while the retained pre-v55 test configuration used
  `ssfx_shadows=(256,1536,0)`. Only that first SSS shadow value was restored to
  `256`, and the matching MCM index was restored to `1`.
- TAA, motion blur, SSS quality, shaders and PiP code were not changed. This is
  a controlled visual comparison for the newly reported moving-shadow trail,
  not an assertion that the SSS setting is already proven to be its cause.
- Previous settings are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260812_140500_sss_aol_profile_fix/settings`.

### Standalone addon fixes

- `Anthology Performance - AOL Transitions` now uses the process-global,
  already parsed `system_ini()` instead of constructing a second
  `CScriptIniFile` for merged `system.ltx`. Transition parsing and callbacks
  are unchanged. New script SHA-256:
  `75F2DD3B35411E81143FBD49DC45494202B07FDCD6437D7C261C455D20DBD421`.
- `Anthology Diagnostics - Actor Load Callbacks` now preformats timing messages
  with `string.format`; the pack's global `printf` accepted the earlier format
  string as a single argument and printed literal `%d` fields. New script
  SHA-256:
  `EC7EBE1BDAD30FB041AADFC1B1A8D4666886508EAF3DD0A7F324F44AFB8DC2F2`.
- Both scripts pass Lua 5.1 bytecode parsing and remain complete standalone
  addons on `D:/ANTHOLOGY_DEV/addons`, installed through the existing top
  priority MO2 junctions. Their previous revisions are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260812_140500_sss_aol_profile_fix/addons`.

### Load-only `seqFrame` attribution

- During the 60 logical precache frames only, `Device.seqFrame` now preserves
  the existing callback order and capture behavior while attributing elapsed
  time to each registered callback object. At the final frame it emits sorted
  `[load-session/frame-callbacks]` lines with total, maximum, call count,
  priority and RTTI type name.
- Outside an active measured precache, the engine continues to call the
  original `CRegistrator::Process` path. The diagnostic therefore adds no
  per-frame profiling or sorting cost to gameplay.
- This measurement is required before moving any additional callback work to
  workers: the current log proves that the main scheduler is not the owner of
  most of the missing `23-26 s`, but does not yet identify which other callback
  is. Parallelizing an unidentified owner would repeat the earlier crash and
  state-corruption risk.

### Dual DX11 build and installation

- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully. The
  executable/PDB pairs are installed in the game `bin` directory:
  - regular DX11 EXE:
    `E9153BA42D58637889C927FA306DD42149E7929983DCC4F649BD8132A7889028`;
  - regular DX11 PDB:
    `31ACD79850BACD1F484A1AE32F53DF3560ED6BFC918EBA5E6D5862C5DE56146F`;
  - DX11-AVX EXE:
    `5F19AC7ED5119A164D571A3D6AE824ED14C5CDB3BF924BB4860E4ACE554967AD`;
  - DX11-AVX PDB:
    `70210D1BFF9D6745DFAD72A3910D78494F6E83A543C6C98AD74BEC1F2B0F5DF4`.
- The replaced v55 regular and AVX pairs are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260812_140300_engine_v56_dual_dx11/bin`.

### PiP status and next test

- The PiP work remains a reviewed implementation plan at
  `D:/ANTHOLOGY_DEV/addons/Anthology PiP Rework/PLAN.md`; it is not installed
  or enabled. The plan covers viewport state isolation, main-render visual
  parity, per-viewport TAA history, PiP sensitivity, effective quality presets,
  hands with NVG/thermal, compatibility toggles, and both DX11 builds.
- Test the same save once from the menu and once by same-level quickload. The
  next log must contain numeric `[load-session/lua-callbacks]` rankings and the
  new `[load-session/frame-callbacks]` ranking. Also compare moving shadows at
  the same location after the SSS rollback; no new game or shader-cache purge
  is required.

## 2026-08-12 - v57 temporal stability, retained LuaJIT and native transition scan

### Evidence from the capture and the latest log

- The supplied `AnomalyDX11AVX.exe 2026.08.12 - 14.30.24.01.mp4` capture is
  1920x1080 and about 11.16 seconds long. It shows temporal trails on moving
  screen-space shadow boundaries in the main viewport; PiP is not active in
  the recorded sequence. The fix therefore targets SSS history only and does
  not change the PiP/SVP renderer or the base TAA shader.
- The latest menu-save reached engine-ready in `37.208 s`, versus `47.820 s`
  in the previous measured v55 run. The remaining large pieces were
  server/Lua `9.272 s`, client spawn `15.760 s`, actor spawn `7.571 s`, and
  precache wall time `22.978 s`.
- The same-level quickload reached engine-ready in `45.506 s`. It spent
  server/Lua `12.253 s`, client spawn `8.699 s`, actor spawn `8.035 s`, and
  precache wall time `32.782 s`. One `CGamePersistent` callback consumed
  `16.248 s`, while the main scheduler accounted for only `2.405 s`; blindly
  increasing scheduler parallelism is therefore not supported by this log.
- The old precache callback profile accidentally accumulated samples across
  load sessions. Its reset now happens when the 60-frame precache begins, so
  the next menu-load and quickload reports are independent.

### SSS ghosting fix without PiP changes

- A complete standalone addon was created at
  `D:/ANTHOLOGY_DEV/addons/Anthology Visual - SSS Temporal Stability` and
  installed as the highest-priority MO2 junction in the active profile.
- Its `ssfx_sss.ps` lowers stale SSS history retention from 95% to 82%, adds
  stronger depth-disocclusion rejection, and rejects residual motion before
  blending the previous shadow mask. Shader SHA-256:
  `53D548CBEF4D0A4612AB78090EC4EE45A3052BD38B8751990F8A9F5B5EA870A6`.
- R4 already skips the SSS pass for `Device.m_SecondViewport.IsSVPFrame()`.
  The addon does not override a PiP shader, does not change PiP matrices, and
  does not touch the independent viewport history.

### Load and frame-time changes

- The pack calls `jit.flush()` together with full Lua collections at the
  loading prompt. Full collections were already coalesced, but the JIT flush
  still discarded every compiled trace immediately before gameplay. During an
  active load session the engine now retains those traces and reports
  `[load-session/lua-jit]`; outside loading, `jit.flush()` behaves unchanged.
- The precache scheduler's temporary batch is reduced from 65,536 to 2,048.
  It still drains the observed 37k-object backlog during the preserved 60
  logical frames, but no longer monopolizes a single CPU core for a massive
  batch while render/resource workers wait.
- The small UI shader cache is no longer destroyed every 200 frames. It is
  invalidated on `OnAssetsChanged()` instead. This removes periodic resource
  recreation and the console/HUD opening spike without retaining stale assets
  after a resource reload.
- The AOL standalone addon now uses the new native
  `line_for_each_section_suffix_prefix("_hud", "ts_", ...)` iterator. C++
  filters the merged INI, so Lua is entered only for actual transition lines.
  The full AOL behavior remains in its independent D-drive addon. Script
  SHA-256:
  `15CCFCB428C61709E4742953CFF0F7F1BD8C2ADE6A33436606C9D14437918069`.
- Actor script-binder and persistent intro-event timings were added only while
  a load session is active. Gameplay MT profiling was enabled in `user.ltx` so
  the next log emits a 300-frame breakdown rather than relying on Task Manager
  core percentages.

### Build, installation and recovery

- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully and were
  installed to the game `bin` directory:
  - regular DX11 EXE:
    `55BE71012394CD4A28569F02D546CD8B9C8B2186BCAA8E2D779ACBAAC5D6159E`;
  - regular DX11 PDB:
    `49D845DBFE19D8A27B36891B9A9A3B3AE879D086E2A90FC946C7B8D08DB23031`;
  - DX11-AVX EXE:
    `6D26295FA833D0507FBFF79FD0BFE171600658D8A5A8AA2F0B58C6FA8061ED1F`;
  - DX11-AVX PDB:
    `09B066778CF9280BFAACE6F445629FB5E11498C54418808F316D1E40E09E5129`.
- All replaced binaries, prior settings, shader, active mod list and previous
  AOL addon are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260812_145747_v57_temporal_load_stutter`.
- The next test does not require a new game. Test one menu-save load, then one
  same-level quickload, and record about 60 seconds of gameplay. The requested
  8-10 second target is not claimed until the new actor-binder and intro-event
  subphase timings identify the remaining serialized work.

## 2026-08-12 - v58 actor-load fast path, MT tail diagnostics and local-light culling

### Evidence from the latest pre-v58 log

- The menu-save load reached engine-ready in `44.800 s`. Its measured phases
  were server/Lua `8.160 s`, native level preparation `2.350 s`, client spawn
  `21.700 s` and final precache `31.300 s`.
- The same-level quickload reached engine-ready in `42.299 s`: server/Lua
  `10.687 s`, native level preparation `0.896 s`, client spawn `8.579 s` and
  final precache `33.565 s`.
- In both sessions the actor script binder alone took about eight seconds
  (`7.800 s` and `7.913 s`). Source inspection identified its fixed Lua loop
  over all `65,534` possible ALife IDs while looking only for phantom objects.
- The quickload's `game_loaded` intro event took `15.101 s`. The event also
  reparsed `ui/game_tutorials.xml`; that old aggregate value does not prove the
  XML was the whole cost, so v58 records XML rebuild/reuse separately.
- Normal gameplay generally spent about `5-7 ms` in FrameMove and `9-12 ms`
  in rendering. The old MT profile nevertheless recorded rare secondary-worker
  waits around `40-58 ms`, with severe samples around `111-176 ms`. The old
  averages could not identify which child task produced those tails.
- `r__framelimit 64` was active and implemented as a gameplay busy wait. It
  also pinned the screenshot near a `16 ms` frame even when rendering had
  headroom. VSync remains disabled.

### Save-load changes

- Added native `alife():iterate_objects_by_clsid(...)`. It filters the existing
  ALife registry in C++ and enters Lua only for matching objects instead of
  probing every possible ID. Matching IDs are snapshotted and each object is
  revalidated before its callback, so a callback may safely release the
  current object without invalidating the registry iterator.
- A complete standalone addon was created at
  `D:/ANTHOLOGY_DEV/addons/Anthology Performance - Actor Spawn` and installed
  through a top-priority MO2 junction. It preserves Anthology's aim settings,
  safe-release manager, `remove_outfits_hack` flag and `on_game_load` callback
  order; only the phantom search uses the native filtered iterator. Script
  SHA-256: `45F296E572FB50C43D51E0CCC4206240F812EB8E6DBE80737CA23DADB479AF83`.
- `game_tutorials.xml` now has a process-local parsed DOM. It is built on first
  use and reused by later save loads in the same process. No persistent cache
  is generated, so changed XML/mod files are picked up normally on restart.
  New `[load-session/tutorial-xml]` lines distinguish rebuild and reuse time.
- The callback diagnostics addon remains independent and now records both
  `on_game_load` and `actor_on_first_update`. Its installed script SHA-256 is
  `688D7D45CE14625365BA1FC0E4B0BB76B378F0CC191F28D51B173D39DC29A07F`.

### Frame pacing and renderer changes

- MT frame profiling now records both averages and per-window maxima for
  pre/post phase work, bones, game tasks, parallel Lua GC and vision. This is
  diagnostic only outside the already enabled 300-frame profile window and is
  intended to identify the remaining intermittent worker wait.
- Removed a shared mutable thread-ID variable from monster vision work and
  added vision-task attribution. The prior static variable was a data race
  when several monsters prepared vision concurrently.
- Anthology's UI custom-static collection is now protected for the existing
  `mt_ui` path, allowing `mt_ui 1` without racing HUD add/remove/update/render.
- Adapted Monolith's per-omnipart visibility test for shadowed point lights.
  Invisible cubemap sides no longer enter shadow rendering, which particularly
  targets campfires and numerous local lights in the Bar. Added a conservative
  shadow LOD floor of `0.02`; shadow quality and SSS temporal fixes are kept.
- Particle waits now use a bounded pause/yield spin helper instead of an
  unbounded hot loop, reducing CPU contention while async particle work
  finishes.
- Active settings are `mt_ui 1`, `mt_task_manager 1`, `r__framelimit 0`,
  `lua_parallel_gc_budget_us 100`, `lua_parallel_gcstep 25`,
  `r2_shadow_omnipart_vischeck 1` and `r2_shadow_lod_min 0.02`.
  PiP and the fixed SSS addon were not modified in this round.

### Build, installation and recovery

- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully and are
  installed in the game `bin` directory. Source and installed hashes match:
  - regular DX11 EXE:
    `E47D655C26334EE847A73E1F0F34D490550ACE29565242C9B8BF856F83CA67EB`;
  - regular DX11 PDB:
    `23E3ED1ECFD2EB723B9208C570BA60978844B8424734B730A346C3E706A4BFEF`;
  - DX11-AVX EXE:
    `84BAF2A67E83B2C351729D488497676340B33B35EBA29C60DA451B9F827E77C2`;
  - DX11-AVX PDB:
    `6EE4CC0F3205D327385DDA8D3FF2894B3E142098CB02583863722B763199EBAA`.
- The complete pre-v58 binaries, settings and profile are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260812_160216_v58_load_fps_stutter`.
- No new game or shader-cache purge is required. The result must be measured
  from a fresh process with the same save, followed by a same-level quickload
  and at least 60 seconds in the same Bar scene. The 8-10 second and large FPS
  targets are not claimed until that test produces the new phase and MT-tail
  measurements.

## 2026-08-13 - v59 spawn streaming, unique model preparation and GC pacing

### Evidence after the interrupted test session

- The latest complete log still showed save loads around `32-33 s` engine
  ready. Static `level.geom` preparation was only `10-12 ms`; delayed visual
  appearance was therefore not caused by serial level geometry I/O.
- Client connection prepared `1,391-4,138` spawn objects and reported
  `862-2,860` model references plus `1,901-4,661` texture references. Source
  inspection showed that repeated object visuals each submitted their own
  preparation task, even though many objects use the same model.
- The legacy spawn-antifreeze thread held every dynamic object until the last
  model in the batch was ready. It also called full `models_PrefetchOne` after
  the newer server path had already built a safe R4 model blueprint, discarding
  much of the parallel preparation and entering renderer state from a worker.
- The remaining repeatable gameplay tails were in Lua GC: normal GC work was
  sub-millisecond, but LuaJIT atomic phases produced approximately `72-183 ms`
  task/wait maxima. System events contain no adjacent WHEA, NVMe/disk or NVIDIA
  driver fault. `Kernel-Power 41` only confirms that the PC later lost power or
  reset without a clean shutdown; it does not identify the PSU or the game as
  the cause of that separate full-system event.

### Loading and delayed-object changes

- Connection resource preparation now deduplicates normalized visual names and
  submits one worker job per unique model instead of one job per spawn object.
  Parent-first packet ordering, model blueprint safety and texture prefetch are
  preserved.
- Prepared connection spawns no longer run the legacy full-model prefetch a
  second time. The safe worker-built blueprint is committed only by the
  existing owner-thread `PublishPreparedClientSpawnResource` path. Runtime
  dynamic spawns without a registered blueprint retain the legacy fallback.
- The spawn-antifreeze queue publishes completed objects in ordered chunks of
  32 instead of an all-or-nothing batch. This lets already-ready parent-first
  objects enter client spawning while later runtime models are still prepared,
  reducing delayed NPC/geometry appearance without changing save data.
- `[client-spawn]` now records object count, unique-model count, texture
  references, resource wait, packet send and total connection time.

### Stutter pacing and exact diagnostics

- Active Lua GC uses a conservative hybrid: Monolith's separate GC task is
  retained, while the incremental step is reduced to IX-Ray's `10` and one
  explicit call per frame (`lua_parallel_gcstep 10`, budget `50 us`, call
  amount `1`). The large LuaJIT atomic phase cannot be divided safely at the
  engine call site, so this change reduces how aggressively the engine drives
  cycles rather than pretending that the microsecond budget can preempt an
  atomic collector phase.
- Experimental `mt_task_manager` and `mt_ui` are disabled for this pack. Their
  measured build did not improve the critical load path and increased the
  concurrent Lua/UI surface. `mt_load_spawn_decode` is also disabled because
  the measured decode stage was only about `55 ms`.
- The `-dbg` command-line switch was removed. `mt_frame_profile 1` remains for
  the next controlled run so GC and task tails can still be compared.
- Added `device():performance_time_ms()` backed by QPC. The standalone
  `Anthology Diagnostics - Actor Load Callbacks` addon now reports fractional
  timings for individual `on_game_load` and `actor_on_first_update` handlers;
  Lua 5.1 syntax validation passed. Installed script SHA-256:
  `85090B19006D0B28EA8CBFDAE21F33E0B37224DDE6A02AB97525CF4141597EEA`.

### Build, installation and recovery

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compiled and
  linked successfully and were installed to the game `bin` directory. Source
  and installed hashes match:
  - regular DX11 EXE:
    `4D2F839A6593E21A18238A9A4055EA3EC1BF32B0D47A9F892574E20D5CB038D9`;
  - regular DX11 PDB:
    `C081BBFBF64A83CEF694881D881E10726FADF9B0A4AC360CC0807FADF495C6F3`;
  - DX11-AVX EXE:
    `008D10DB82BD706ADAE3820F75E718D7FAC66D94B83BB0FBF105A760B6108E4A`;
  - DX11-AVX PDB:
    `26CF5235E95CA0DDDC0C34EF9132A8248105A6A2F144325E6D3111C4A3EED97D`.
- The pre-v59 files are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260813_110455_v59_streaming_gc_diagnostics`; the
  installed v59 package and symbols are mirrored at
  `E:/ANTHOLOGY_BACKUPS/20260813_114213_v59_installed_streaming_gc`.
- PiP, SSS, the temporal shadow fix, saves and shader cache were not modified.
  No new game is required. The next test is one menu-save load, one same-level
  quickload and 60 seconds of movement in the same scene; the 8-10 second load
  target is not claimed until that log is measured.

## 2026-08-13 - v60 deterministic level-cache reuse and movement-safe Lua GC

### Evidence from the v59 test log

- Native resource preparation is working: 2,079 client spawns were reduced to
  266 unique models and 690 texture references, with the resource barrier only
  `126.75 ms`. R4/CFORM preparation itself was roughly `0.15-0.34 s`.
- The repeatable save-load cost is now dominated by two serial mod callbacks:
  Western Goods readable texture discovery used `5.88-6.30 s`, while its
  random-dialog DXML scan used `1.33-1.40 s` on every load.
- During ordinary movement the renderer stayed near `10 ms`, but LuaJIT GC
  periodically produced non-preemptible atomic tails of `196-215 ms`. These
  stalls explain the 50-90 FPS oscillation and walking microstutter more
  directly than Windows assigning the main/game workers to cores 2 and 7.
- The R4 and CFORM cache pressure test ran at level teardown, before the old
  level released the rest of its memory. That transient high-water mark could
  immediately evict the package intended for the next same-level quickload.

### Changes

- R4 and CFORM now try an exact level/identity restore before applying memory
  pressure eviction. Retention no longer samples RAM at the teardown high-water
  mark; each cache is capped at two packages as a separate safety bound.
- Added an Anthology LuaJIT API step that completes incremental marking but can
  stop immediately before the atomic phase and suspend automatic allocation
  checks. While the actor is actively moving, the engine defers that atomic
  phase until movement stops or a configurable 30-second safety limit expires.
  Full load-end GC always resumes the collector first. The setting is exposed
  as `lua_gc_movement_defer_ms` (`0` disables it).
- Added a read-only `ui_texture_ids(prefix)` engine export over the UI atlas
  already parsed in memory.
- Added standalone `Anthology Performance - Western Goods Load Cache`: readable
  pages now use the parsed atlas instead of reparsing every textures-descr XML
  for every item. The current single `<rand_text>` dialog is parsed once per
  process instead of walking the entire gameplay dialog list on each load.
- No PiP, SSS, shader, save or unrelated gameplay script was changed.

### Build, installation and recovery

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compiled and linked
  successfully and are installed in the game `bin` directory. Source and
  installed SHA-256 hashes match:
  - regular DX11 EXE:
    `FE4B1B6410AF33AB93B521E85783FC7DCCD266D8B4D2D4E75E37FDC7ECA8D3E8`;
  - regular DX11 PDB:
    `BCB737CFA8843E34E40C53928B933B841E12C3D9878CF88180702EB75FF0C0F0`;
  - DX11-AVX EXE:
    `4A1946F2AA2F2872481AB718B6B995649A24224065DD45D6D500C614252C4269`;
  - DX11-AVX PDB:
    `E1996A33AFAEFF961C2B53DA0545F62D92BDF98585B71B2BF267F722FEA07B67`.
- The standalone patch source is mirrored at
  `D:/ANTHOLOGY_DEV/addons/Anthology Performance - Western Goods Load Cache`,
  connected to MO2 by a junction and enabled in the active HARD profile. Both
  scripts pass Lua 5.1 syntax validation and match the tracked source hashes.
- Removed the launcher `DBG` token and `-dbg`; disabled the completed
  `mt_frame_profile` diagnostic and the accidental `470 FPS` cap. Active GC
  settings are `lua_parallel_gcstep 75`, budget `100 us`, one call per frame
  and `lua_gc_movement_defer_ms 30000`.
- Pre-change recovery is at
  `E:/ANTHOLOGY_BACKUPS/20260813_134804_v60_pre_cache_gc_modfix`; the exact
  installed v60 package is mirrored at
  `E:/ANTHOLOGY_BACKUPS/20260813_141105_v60_installed_cache_gc_modfix`.
- No new game and no shader-cache purge are required. The result should be
  tested from a fresh process, then with a same-level quickload and 60 seconds
  of uninterrupted walking; measured 8-10 seconds is not claimed before that
  real run produces a log.

## 2026-08-13 - v61 emergency removal of movement-deferred GC

- The first v60 gameplay report showed an immediate collapse to roughly
  `10 FPS`, so that build is not accepted for further testing.
- Removed the complete `LUA_GCSTEPDEFERATOMIC` LuaJIT extension, its suspended
  collector state, movement timer and console setting. The LuaJIT and
  `CLevel::LuaGC` sources now match the last pre-v60 revision exactly.
- Restored the conservative v59 runtime values: `lua_parallel_gcstep 10`,
  `lua_parallel_gc_budget_us 50`, one call per frame. Debug/profile logging
  remains disabled and there is no engine FPS cap.
- Kept only the independent loading changes: R4/CFORM restore-before-pressure
  ordering and the standalone Western Goods XML patch. The fresh log confirms
  the Western Goods readable callback fell from approximately `5.9-6.3 s` to
  `0.22 ms`; it does not execute per gameplay frame.
- Before v61 finished building, the installed game binaries were immediately
  rolled back to the known pre-v60 v59 hashes so the rejected 10-FPS binary
  could not be launched again by accident.

### Build and installation

- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully after the
  GC removal. Installed files match the source build hashes:
  - regular DX11 EXE:
    `58D55A6ADB29E94317BDDE3A9E8E46D841564C8C09C6263A3B94D4F600650E1A`;
  - regular DX11 PDB:
    `EE22E81AA54867DD2A4471D75DC8D11ABC80699C7CA863EA5C4C821389937E5C`;
  - DX11-AVX EXE:
    `E67E81DC9A44B9CEA74A02AE57D12D027AA0AB1D07AA2C3279A463C1A6C93AB4`;
  - DX11-AVX PDB:
    `982699652C638DB62CDEB5D03B6AB3BC9187793A436959086246B11C8AF99342`.
- The rejected v60 installation is recoverable only for diagnosis at
  `E:/ANTHOLOGY_BACKUPS/20260813_142300_v61_pre_gc_revert`. The active v61
  binaries and settings are mirrored at
  `E:/ANTHOLOGY_BACKUPS/20260813_143200_v61_installed_gc_reverted`.

## 2026-08-13 - v62 frame-pacing isolation

### Evidence and scope

- The v61 gameplay report showed that loading improved enough to keep the
  current solution unchanged, while ordinary play could still jump from about
  `80` to `40 FPS`. This round does not modify R4/CFORM caches, spawn/resource
  preparation, precache, Western Goods, PiP, SSS or any gameplay addon.
- Historical MT frame samples show ordinary work near `15-17 ms`, interrupted
  by secondary-task waits around `50-100+ ms`. The Lua GC worker accounted for
  the recurring long tail; Windows System events around the latest run contain
  no adjacent WHEA, display-driver, disk or NVMe fault.
- `LUA_GCSTEP` installs a new automatic allocation threshold before returning.
  That allowed a later Lua allocation to enter LuaJIT's non-preemptible atomic
  phase from FrameMove even though the pack enabled the separate MT GC task.

### Frame-pacing changes

- After every renderer-overlapped incremental GC step, the automatic LuaJIT
  allocation trigger is stopped again. The next explicit MT step still runs
  normally because `LUA_GCSTEP` supplies its own threshold. Full load-end GC is
  preserved, and normal automatic GC is restarted before the level VM is torn
  down. This does not defer or skip an atomic phase; it keeps that phase on the
  already synchronized MT task instead of letting it appear unpredictably in
  FrameMove.
- Runtime GC defaults and active settings use the previously less aggressive
  `lua_parallel_gcstep 25`, `lua_parallel_gc_budget_us 100`, call amount `25`.
  The rejected movement-dependent LuaJIT collector from v60 remains completely
  absent.
- MT telemetry now preserves all parts of the single worst frame in each
  1,200-frame window and divides the game worker into scheduler,
  `seqParallel`, and `seqFrameMT`. Four compact lines are emitted roughly every
  20 seconds at 60 FPS, reducing diagnostic log I/O versus the old 300-frame
  interval. `mt_frame_profile 1` is enabled for the controlled test.

### Build and recovery

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Source-build and installed hashes match:
  - regular DX11 EXE:
    `77AFEF4A9DFFDF7527C09B81B798B7663BB02FE1689F166EF2EB42ED89D65931`;
  - regular DX11 PDB:
    `94F74E85DD4412CA6959E2A3E8A7F6E2256F23795AF77DEE5E8DBB86B38393FD`;
  - DX11-AVX EXE:
    `3716322FC0C02195CE914E33B13645DEEA6C9CB23AB57D97E308E898A992D9F7`;
  - DX11-AVX PDB:
    `5F4B0B2687F7D7F59F6F18276929AE5408CBE7C3DAA98ED85CBE2EBA31C3A596`.
- Pre-change v61 binaries and settings are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260813_145429_v62_pre_frame_pacing`.
- The exact installed v62 binaries, symbols and settings are mirrored at
  `E:/ANTHOLOGY_BACKUPS/20260813_151000_v62_installed_frame_pacing`.
- No new game and no shader-cache purge are required. The user should load the
  same save and move through the same heavy scene for at least 60 seconds; the
  new `worst-frame` and `worst-game-parts` lines will distinguish any remaining
  renderer, GC, scheduler, callback or bone-calculation tail.

## 2026-08-13 - v63 rejection and complete rollback of v62

### Crash and stutter evidence

- v62 is rejected. The gameplay log ends with `[SCRIPT ERROR]: not enough
  memory`; the stack reaches `lj_err_mem` while Lua was growing its stack from
  the keyboard callback path. Stopping the automatic collector after every MT
  step allowed the Lua heap to grow until allocation failed.
- The same log proves that this mechanism damaged frame pacing before the
  crash: a worst frame waited `315.17 ms` for secondary work while the Lua GC
  task consumed `311.17 ms`; a later sample waited `80.30 ms` with `88.85 ms`
  in Lua GC. These are visible stutters, not a GPU or storage diagnosis.
- MCM's options reset rewrote `r__framelimit` to `476`. That value is not a
  60-FPS lock, but the active setting has still been restored to `0`.
  `rs_v_sync` and `rs_refresh_60hz` remain off.

### Corrective action

- Removed the v62 `LUA_GCSTOP`/`LUA_GCRESTART` lifecycle changes completely.
  Lua GC ownership and all executable source now match v61 exactly.
- Removed the v62 per-stage/worst-frame diagnostic instrumentation as well, so
  none of its extra timing atomics remain in normal play. Runtime diagnostics
  are disabled with `mt_frame_profile 0`.
- Restored the last smooth runtime values in the active `user.ltx`:
  `lua_parallel_gcstep 10`, budget `50 us`, one call per frame. The installed
  regular DX11 and DX11-AVX binaries were immediately restored to the exact v61
  hashes before another launch could use v62.
- The accepted v61 loading solution, R4/CFORM caches and standalone Western
  Goods patch are retained unchanged. No PiP, SSS, shader or gameplay addon was
  modified in this rollback.

### Build, installation and recovery

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compiled and linked
  successfully after the complete source rollback. The fresh build artifacts
  were retained only as verification because PE/PDB build metadata changes
  their hashes even when the executable source is identical.
- The installed game deliberately keeps the exact known v61 pair instead of
  replacing it with newly timestamped equivalents:
  - regular DX11 EXE:
    `58D55A6ADB29E94317BDDE3A9E8E46D841564C8C09C6263A3B94D4F600650E1A`;
  - regular DX11 PDB:
    `EE22E81AA54867DD2A4471D75DC8D11ABC80699C7CA863EA5C4C821389937E5C`;
  - DX11-AVX EXE:
    `E67E81DC9A44B9CEA74A02AE57D12D027AA0AB1D07AA2C3279A463C1A6C93AB4`;
  - DX11-AVX PDB:
    `982699652C638DB62CDEB5D03B6AB3BC9187793A436959086246B11C8AF99342`.
- The corrected installed state is mirrored at
  `E:/ANTHOLOGY_BACKUPS/20260813_152354_v63_installed_v61_rollback`.
- No new game or shader-cache purge is required. Test from a fresh process;
  the game itself has no active 60-FPS limiter in this configuration.

## 2026-08-20 - v64 Lua VM owner-thread safety

### Crash and stutter cause

- The latest post-load crash reaches `lj_gc_step` while Lua UI/luabind work is
  active. The previous `lua_parallel_gc` implementation submitted a second
  nested worker from `GameThread` and then continued into `seqFrameMT`; both
  paths could touch the single LuaJIT VM concurrently.
- LuaJIT's allocator and collector are not safe for concurrent access to the
  same state. The renderer eventually waited for that worker at frame end, so
  the implementation could also turn a long atomic GC phase into the observed
  periodic secondary-wait spike instead of removing it.

### Corrective action

- Removed the renderer-overlapped Lua GC task. Incremental collection now runs
  from `CLevel::OnFrame` on the Lua VM owner thread. The existing
  `lua_parallel_gcstep`, call-count and microsecond-budget settings are retained
  as compatibility controls and still split normal collection into small
  explicit steps; they no longer mean concurrent VM access.
- The `mtLUA_GC` flag can no longer move `script_gc`, script physics commander
  work or the collector into `seqParallel`. Safe native renderer, bone,
  particle, scheduler and vision jobs remain multithreaded.
- `mt_frame_profile` still attributes Lua GC time separately through explicit
  owner-thread timing, allowing later test logs to distinguish a remaining Lua
  atomic phase from scheduler, render or vision work.
- Full collections coalesced at the end of a load session remain unchanged.
  No GC stop/restart mechanism was reintroduced, so the v62 out-of-memory
  regression remains absent.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compiled and linked
  successfully. Source-build and installed hashes match:
  - regular DX11 EXE:
    `79819A97BC3484270504D183B56CB8BF48D3AA3ED4D5272CA506FC3E6EBC3441`;
  - regular DX11 PDB:
    `26D92BC9F8C6029D27B42FD0343BA753A228C508EF2E108CA9F67D8AC61A366E`;
  - DX11-AVX EXE:
    `844CC7D8BB11FEF092BB60293A1160331D8F99BD179374CDB8E9BDAE7B46F111`;
  - DX11-AVX PDB:
    `21F7ECEA61D4680B81F4125380AAAB9992E6FEA815AFE8601FF378C050D2D317`.
- The exact previous v61 binaries and active `appdata/user.ltx` are recoverable
  from `E:/ANTHOLOGY_BACKUPS/20260820_214337_v64_pre_owner_lua_gc`.
- Runtime settings and all gameplay addons were deliberately left unchanged;
  the four standalone Catspaw/Dot Marks/Tactic Compass/Interactive PDA patches
  remain disabled so this GC ownership change can be measured independently.
- No new game or shader-cache purge is required.

## 2026-08-20 - v65 periodic-tick isolation and runtime sound-prefetch pause

### Capture and log evidence

- The latest 12.32-second 60-FPS capture contains three distinct multi-frame
  presentation stalls during continuous camera motion: approximately 67 ms at
  2.92 s, 83 ms at 6.60 s and 50 ms at 10.28 s. Their 3.65-3.68-second spacing
  confirms a periodic CPU/runtime event rather than ordinary FPS variance.
- The overlay reports only 18-35% GPU use around the stalls. The previous log
  had no `mt-frame/profile` samples because the active `appdata/user.ltx` had
  reverted to `mt_frame_profile 0`.
- The same active file had also reverted from the last smooth GC controls to
  `lua_parallel_gcstep 76`, 25 calls and a 250-us per-frame budget. It is now
  restored to step 10, one call and 50 us, with busy-hands debugging disabled.
  The compact 300-frame MT profiler is enabled for the next controlled run.
- Sound prefetch resumed as soon as the load session ended with 36,796 optional
  sources still queued. Runtime prefetch then opened/prepared one unrelated OGG
  every 10 ms throughout play, despite all level-requested sources already
  being promoted during loading.

### Corrective action

- Bulk optional sound prefetch now remains paused while a level is active. The
  loader still prepares requested sound metadata, and any unprepared sound can
  still use the existing synchronous demand path. Bulk prefetch resumes after
  disconnect/in the main menu, removing sustained background I/O and decode
  contention from gameplay.
- No gameplay addon, PiP, SSS, renderer, save format or load-cache mechanism was
  changed in this isolation build.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully. Installed
  artifacts match their source-build hashes:
  - regular DX11 EXE:
    `09FDFC777B3DD34D4BD8F70239FAA6E9D228A01747802FA3C878F4466C8D629E`;
  - regular DX11 PDB:
    `9371764E6486714D8C0DB8FFE97D11C1B27FCBA13F189AF5A9C0266A12CA1ADE`;
  - DX11-AVX EXE:
    `5AAD6F652A2AC5FE5124F329D3A57D351355498FC7D6906FCF3A13404F8CAB9D`;
  - DX11-AVX PDB:
    `1F3F2D23C346A6A6DF3E4CB0F38198BE9174BCBE33B75D8C65D85813C5CCEFF8`.
- The exact previous v64 binaries, symbols and corrected active settings are
  recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260820_221350_v65_pre_sound_prefetch_pause`.
- No new game or shader-cache purge is required. Test the same save and record
  at least 20 seconds of continuous camera motion; the next log will contain
  `[mt-frame/profile]` aggregates even if a residual tick remains.

## 2026-08-20 - v66 serialized idle-time Lua GC

### v65 profile result

- The first fresh run with `mt_frame_profile 1` showed that pausing optional
  sound prefetch did work: it remained suspended throughout the active level
  and resumed only after disconnect. The unchanged visible tick therefore was
  not caused by background sound reads.
- The profile identified the periodic hard stall directly. Lua GC maxima were
  182.04, 210.23, 225.34, 200.88, 217.58 and 238.57 ms; the corresponding
  worst complete frames were 205.59, 244.46, 260.00, 264.60, 245.92 and
  271.29 ms. Those spikes are large enough to explain the visible recurring
  freeze without relying on an overlay estimate.
- v64 made access to the single LuaJIT VM safe, but running every GC step on
  the frame/VM-owner thread exposed LuaJIT's indivisible long collector phases
  directly as frame stalls. A smaller step or budget can delay such a phase;
  it cannot interrupt it after the phase has begun.

### Corrective action

- Adapted the current Monolith idle-time ordering instead of restoring the old
  nested-task implementation. `GameThread` now completes `seqFrameMT` first,
  then performs incremental Lua GC on that same secondary thread while the
  main thread is rendering. Script MT callbacks and GC therefore never touch
  the LuaJIT VM concurrently.
- `CLevel::script_gc` keeps script-physics commander work on the owner thread,
  but no longer repeats collection there when parallel GC is enabled. The
  ordinary non-parallel fallback is unchanged.
- Load sessions still defer this background collector, preserving the stable
  loading path and the existing end-of-load full-GC coalescing.
- Runtime controls are set to step 75, at most 25 calls and a 5000-us overlap
  budget. Work stops sooner when rendering ends. `mt_frame_profile 1` remains
  enabled so the next test can verify both GC time and the worker-wait tail.
- This does not split LuaJIT's atomic collector internals, which would require
  coordinated mutator-barrier changes. It instead moves that unavoidable work
  into an established renderer-overlap window without reintroducing concurrent
  Lua execution.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully. Installed
  artifacts match their source-build SHA-256 hashes:
  - regular DX11 EXE:
    `DF0BE7E5E4D3C1A3C8E5A54512A6F6C3DCE4D3C9B69A6018DE0E3B1833B87722`;
  - regular DX11 PDB:
    `5EC387E13F1E2EDAA21E06926276B13FE4095CBAEDCD69481D6E775E11B31A1A`;
  - DX11-AVX EXE:
    `1297CEA4FE167D91615093CEC0EE924816A72AAD19D77BEB795094B80E1BEDA7`;
  - DX11-AVX PDB:
    `CFB18C518B2D35A2DAA5B8E0931B60A4BEDB87CD2044BAC8392005EF3AE699C6`.
- The exact previous v65 binaries, symbols and active runtime configuration
  are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260820_224000_v66_pre_serialized_idle_gc`.
- No new game or cache purge is required. Test from a fresh process with the
  same save so the next profile is directly comparable to v65.

## 2026-08-20 - v67 mod performance and persistent A-Life groups

### Frozen v66 baseline and profile evidence

- The user-confirmed smoother v66 state is frozen before this work as annotated
  tag `anthology-v66-known-good-20260820`, pointing to commit
  `d40c597c5de35999f9879e59b31dc2c1eea1513f`.
- Its complete installed binaries, symbols, active `user.ltx` and control log
  are also recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260820_2305_v66_known_good`.
- The v66 active-game profile no longer shows the v65 Lua-GC catastrophes. In
  representative heavy-scene samples, complete/frame-render/worker-wait means
  were approximately 23-30 / 8-9.5 / 14-20 / 0.4-0.7 ms. Lua GC commonly used
  about 5 ms but overlapped rendering. The remaining cost is therefore split
  between main game/Lua work and rendering rather than a stalled MT worker.

### Standalone WTF 4.2 performance patch

- Added `Anthology Performance - WTF 4.2` as a standalone addon. The original
  `[QUE] wtf 4_2` files are untouched.
- WTF task status processing no longer repeats actions, subtask traversal,
  callbacks and map-target work once per rendered frame for every active task.
  Each task instead receives a deterministic staggered 75-125 ms update slot;
  terminal completion or failure is still returned immediately.
- The cadence table is runtime-only and is rebuilt on load, so no timer based on
  `time_global()` is serialized into saves. Finished tasks remove their slot.
- The addon source is tracked under `modpack-patches`, its working copy is at
  `D:/ANTHOLOGY_DEV/addons/Anthology Performance - WTF 4.2`, and MO2 consumes it
  through a directory junction. Lua 5.1 syntax validation passes.

### Persistent approximately 300 m A-Life groups

- Added `Anthology A-Life - Persistent 300m Groups` as a separate addon. At the
  first actor update it applies `al_switch_factor 0.10` and switch distance 330,
  yielding approximately 297 m online and 363 m offline thresholds with
  hysteresis. It has no per-frame callback and adds nothing to a save.
- `CSE_ALifeOnlineOfflineGroup::update()` now translates each offline member by
  the squad's movement delta instead of overwriting every member with the same
  squad-centre coordinate. This preserves an established formation across
  offline/online transitions and removes the engine-side one-point respawn.
- Current-level member positions are synchronized against the AI map after the
  translation. Invalid graph vertices and per-member points outside the AI mesh
  have guarded fallbacks to the squad centre rather than unsafe access.
- Fixed an independent undefined-behaviour bug in
  `CSE_ALifeGroupAbstract::synchronize_location()`: it previously dereferenced
  the loop iterator after the iterator had already reached `end()`.
- The addon source is tracked under `modpack-patches`, its working copy is at
  `D:/ANTHOLOGY_DEV/addons/Anthology A-Life - Persistent 300m Groups`, and MO2
  consumes it through a directory junction. Lua 5.1 syntax validation passes.

### NPC dynamic-light budget for the expanded online radius

- Actor flashlight behaviour is unchanged. NPC torch glow remains visible at
  range, while its expensive dynamic spot/omni lights are active only within
  `r__npc_torch_dynamic_distance` (75 m in the active configuration).
- The lights are enabled again automatically as an NPC approaches, including
  nearby and indoor encounters. The new console variable accepts 0-300 m and
  can be tuned without another build; `ai_use_torch_dynamic_lights on` and
  `r__optimize_torch 1` remain enabled.
- This bounds the render cost of keeping more NPCs online instead of paying for
  dynamic shadow-casting lights across the entire approximately 300 m radius.

### Installation, rollback and test scope

- Both standalone addons are enabled at the top of the active
  `Anthology 2.1 HARD Сложный` MO2 profile. The earlier Catspaw, Dot Marks,
  Tactic Compass and Interactive PDA experimental patches remain disabled; no
  unrelated mod, PiP, SSS or script was modified.
- Both `DX11|x64` and `DX11-AVX|x64` compiled and linked successfully after the
  final A-Life safety guard. Installed artifacts match their build SHA-256:
  - regular DX11 EXE:
    `C193E20590D0E1445BA9F0F5AE8B95DE313409283C9A804380D3B7D76C44557F`;
  - regular DX11 PDB:
    `B42D5D389461683F634D7AA0F318A75EFC1F70417ADFC30BC7AC396C4E0E00F7`;
  - DX11-AVX EXE:
    `EA991944A023681BEE5CEBC350E7CA5E00B99ADF2207B9717A7488AA2D703105`;
  - DX11-AVX PDB:
    `C2E9C7C00CDA23AE711B22CBB0AA78A110AF6DDF2C1A7847CB628B9097D67764`.
- The pre-v67 engine sources, original WTF script and MO2 mod list are backed up
  at `E:/ANTHOLOGY_BACKUPS/20260820_2330_v67_pre_perf_alife`.
- No new game or cache purge is required. Test the same save: active WTF tasks,
  a populated smart terrain, leaving and returning to an NPC group, and NPC
  flashlights at night/indoors. A group already collapsed by an older engine
  must first spread normally while online; subsequent transitions preserve the
  resulting member offsets.

## 2026-08-21 - v68 four-addon stutter isolation

### Scope and isolation

- Added four new standalone v68 MO2 addons rather than re-enabling the earlier
  experiments: Catspaw PAW, Interaction Dot Marks, Tactic Compass and
  Interactive PDA. The old four patches remain present but disabled.
- Working copies live under `D:/ANTHOLOGY_DEV/addons` and the active HARD
  profile consumes them through directory junctions. Original addon files and
  every unrelated mod remain untouched.
- The complete pre-v68 mod list, old MO2/working patches and source scripts are
  recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260820_235423_v68_pre_four_addons`.

### Catspaw PAW

- `item_radio.scan_online_sources()` previously probed all 65,534 possible IDs
  every five seconds while the RF detector was active. It now visits actual
  online binder objects and explicitly tracked RF targets only. This removes a
  periodic global lookup sweep without changing RF scan frequency or UI rate.
- Temporary-pin cleanup now returns immediately until the earliest known pin
  can expire, and its maintenance timer is staggered away from the former
  five-second collision point.

### Interaction Dot Marks

- Fixed a movement-state error: the reference actor position was never
  advanced, so after the first movement the addon could rebuild pickup tables
  and perform movement scans indefinitely.
- Current targeting remains frame-rate driven. Near scans are limited to at
  most 5 Hz, the large predictive scan runs at most every three seconds and
  only while travelling, and independent scanners receive staggered initial
  deadlines instead of firing together immediately after loading.

### Tactic Compass

- Direction, map-marker motion and waveform animation still update every
  rendered frame. The rejected experimental 33 ms/30 Hz render cap is not used.
- MCM layout reads and per-category marker definitions are cached for 250 ms;
  reusable UI vectors/rectangles reduce transient Lua allocations.
- A save containing `init_markers` skips the duplicate 65,534-ID marker scan.
  New games or legacy saves without this state retain the original one-time
  catalogue build.
- Stale map-spot validation is processed eight IDs per frame every two seconds
  instead of walking every marker every frame. Fast and medium enemy radius
  scans are staggered and cannot execute on the same frame.

### Interactive PDA

- Disabled an invisible synthetic workload which grew a table toward 65,534
  entries and serialized it into every save. Existing `pda_x_t` data is ignored
  on load and removed on the next save.
- The potential task-giver catalogue is built from actual `SIMBOARD.squads`,
  persisted, and maintained by NPC spawn callbacks instead of probing every
  possible ALife ID on every load.
- Remote trading reuses the classified catalogue. Raid and local status
  searches iterate `SIMBOARD.squads`; the emission sender loop is bounded and
  cannot spin forever when fewer than four eligible senders exist.
- Task, active-task and cooldown timers are staggered after load. The permanent
  actor-update callback associated only with the removed synthetic workload is
  no longer registered.

### Validation and test scope

- All eleven overridden Lua scripts pass the Lua 5.1 parser after preserving
  the two source files which require Windows-1251 encoding.
- Repository and `D:/ANTHOLOGY_DEV/addons` SHA-256 hashes match for every v68
  file. Control hashes also confirm the source Catspaw, Dot Marks, Compass and
  Interactive PDA mods were not edited.
- No engine rebuild, new game or cache purge is required; installed v67 DX11
  and DX11-AVX binaries remain unchanged. Use the same save, create one fresh
  save after loading so the obsolete Interactive PDA payload is purged, then
  measure continuous movement for at least two minutes with the RF detector,
  compass, Dot Marks and Interactive PDA enabled.

## 2026-08-21 - v69 CoP smart jobs and GPU-bound base performance

### Fresh transition/profile evidence

- The Skadovsk/Jupiter run proves the secondary game worker is active. Stable
  frames spend approximately 7-11 ms in serial frame work, 9-13 ms in the game
  worker and only 0.3-1.6 ms waiting for that worker. More worker threads alone
  cannot double this result.
- The same samples spend 16-30 ms inside `seqRender`; on Jupiter the common
  range is 22-30 ms. This is the current 33-45 FPS limiter, while the serial
  CPU side by itself would permit roughly 90-110 FPS.
- The active renderer profile had SSFX settings well above the engine defaults:
  AO 8, SSR 4, directional/omni SSS 18/6 and both material/terrain POM at 36
  samples. The active file also retains high sun, high volumetric sunshafts,
  volumetric lighting/smoke, water reflections and a 110 m detail radius.

### May Monolith dynamic-HOM adaptation

- Adapted themrdemonized Monolith commit `ec01e1169e` (2026-05-25). With
  `r__hom_dynamic on`, dynamic world visuals fully hidden by the level HOM are
  rejected before render packets are built. This targets populated interiors
  and bases where many NPCs/objects are behind solid geometry.
- The normal-world-pass guard from the source change is retained, so HUD and
  PiP-specific render queues are not subjected to this culling path. The path
  is independently reversible at runtime with `r__hom_dynamic off`.

### Balanced RTX 5070 / 1080p GPU profile

- Applied IL 16, AO 4, SSR 2, directional/omni SSS 12/4, material POM 16 with
  refinement disabled and terrain POM 12 to the actual active
  `appdata/user.ltx`.
- Texture quality, resolution, sun quality, volumetric lighting, water
  reflections, vegetation density/radius, PiP and the temporal SSS shadow fix
  are unchanged. The exact profile is tracked in
  `modpack-patches/Anthology Performance v69 - RTX 5070 1080p GPU Profile`
  and mirrored under `D:/ANTHOLOGY_DEV/addons`.
- Shader quality values participate in the shader cache key. New variants may
  compile during the first run, but a manual cache purge is not required.

### CoP-style smart job placement

- The v67 formation translation was the reason the reported centre spawn did
  not improve: translating a formation between differently shaped smarts put
  members outside the destination AI mesh, and its safety fallback collapsed
  them back to the squad centre.
- Restored the stock server-group movement behaviour and moved placement to the
  original CoP-style smart-job stage. The standalone addon wraps
  `smart_terrain.setup_gulag_and_logic_on_spawn` and repeats the existing
  `db.spawned_vertex_by_id` redirect for already initialized jobs. Returning
  NPCs now enter online at their assigned camp, guard, patrol or work vertex.
- The wrapper performs no per-frame scan and writes no new save state. Existing
  saves are supported after one offline -> online cycle; no new game is needed.
- `CALifeSimulator::set_switch_factor` is exported to Lua. The addon now sets
  factor 0.10 and centre distance 330 directly, removing the recurring
  `Unknown command: al_switch_factor` log error while retaining approximately
  297 m online / 363 m offline thresholds.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compiled and
  linked successfully. Installed files match their build hashes:
  - regular DX11 EXE: `184E5558B889771223E34E11F39466F22E588F71F8FC94A72DFED05F526556D2`;
  - regular DX11 PDB: `498E12D2AC54DB8BC14C597E87701A3993DE31FE7AD0856BB72E7E85F3484EF4`;
  - DX11-AVX EXE: `3058C133CDFD9D06731158AEB91AA2A4791A066DCB93C341F9373F3E1BA2A4E9`;
  - DX11-AVX PDB: `05D6485922660ED7F76E7A461A6FB0771BF8A20384D0D16E68673FEF39981224`.
- `Anthology A-Life v69 - CoP Smart Jobs 300m` is enabled in the active HARD
  MO2 profile and the obsolete v67 A-Life addon is disabled. All original mod
  scripts remain untouched.
- Complete pre-v69 sources, binaries, symbols, active settings, MO2 list and
  old A-Life addon are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_0120_v69_pre_cop_jobs_gpu_profile`.

## 2026-08-21 - v70 stutter rollback, covered world warm-up and menu 60 FPS

### Fresh v69 evidence and dynamic-HOM correction

- The latest gameplay profiles show healthy averages around 16-20 ms but
  irregular GameThread/secondary-wait tails of roughly 50-190 ms. Lua GC alone
  reaches approximately 30-46 ms in several 300-frame windows, matching the
  reported ticks rather than a permanently inactive secondary worker.
- The active runtime profile had raised the GC overlap budget to its maximum
  5000 us with a 76-unit step and 25 calls. v70 uses a 20-unit step, eight calls
  and a 1000-us budget to reduce the duration of an indivisible LuaJIT step.
- The experimental per-child-visual HOM path from Monolith commit `ec01e1169e`
  duplicated the renderer's existing once-per-renderable spatial HOM test.
  Upstream commit `27d0968b85` later disabled this experiment by default for
  the same branch. v70 follows that correction with `r__hom_dynamic off`; the
  established renderable-level HOM rejection remains active.

### Covered normal-world warm-up

- Added `load_world_warmup_ms` (0-10000, default and active value 5000). Once
  the legacy precache ends and load queues first drain, the engine keeps the
  loading screen and input block active while executing normal
  `dwPrecacheFrame == 0` world, script, scheduler and render frames.
- The phase finishes only after both the time budget and queue-drain conditions
  are satisfied. Lazy geometry/textures, online objects and script work are
  therefore allowed to settle before the player sees or controls the world.
- This does not restore 60 expensive world renders or change the sparse
  precache callbacks. The log now records
  `[load-session/warmup] begin/complete`, including frame count and elapsed
  wall time, so the next user test can separate covered warm-up from loading.

### Menu-only limiter

- Added `r__menu_framelimit` (0-240, default and active value 60). It applies to
  the main/pause menu only; `r__framelimit 0` leaves gameplay uncapped and an
  active load session bypasses the menu limiter.
- Replaced the old full-frame ECO busy loop with a coarse `Sleep` plus a short
  scheduler-yield tail. This limits menu GPU/CPU load without burning a core
  while waiting for the next 60 Hz frame.

### Installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `90D141B320E9243A2E11C09D092DCAF2DFE1E5CBA89F4987D5544EF86523A432`;
  - regular DX11 PDB:
    `DF29D8D56963F4796218701574F5538AAB4046E5123553B3F1AD28F391B3859D`;
  - DX11-AVX EXE:
    `864FDD55FC62BED05B3352972AE6F116065AE01AAC74826432388826A578DAAB`;
  - DX11-AVX PDB:
    `776BEC98D00688CC3D3C080D4C04D995E5DCE17C955F69D2A18C045D5342F60D`.
- The exact active settings are mirrored as the standalone audit profile
  `D:/ANTHOLOGY_DEV/addons/Anthology Performance v70 - Stutter Warmup Menu 60`
  and tracked under `modpack-patches`; no unrelated addon script was changed.
- Complete pre-v70 sources, binaries, symbols, runtime config, MO2 list and
  control log are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v70_pre_stutter_warmup_menu60`.
- No new game or shader-cache purge is required. Test the same save and keep
  `mt_frame_profile 1` enabled for direct before/after tail comparison.

## 2026-08-21 - v71 covered-warmup deadlock hotfix

### Confirmed cause

- The first v70 test log reached `intro_start game_loaded` and then recorded
  `[load-session/warmup] begin target=5000 ms`, but never recorded the matching
  completion line. The session was finally cancelled on disconnect after
  43315 ms. This confirms a load-session deadlock rather than a save crash.
- v70 required `queues_drained` on every call to `LoadSessionTryFinish`.
  Normal `dwPrecacheFrame == 0` world frames continuously enqueue runtime game
  events, so the warmup timer path was never entered again. Because input is
  deliberately blocked while a load session is active, the `Zone awaits`
  screen could not respond to a key.

### Correction

- Level readiness, player control and empty load queues remain mandatory before
  the covered warmup starts. This preserves the original load-safety barrier.
- After that barrier has been crossed once, runtime queues no longer gate the
  timer. The five-second wall-time budget now always reaches completion and the
  load generation is finalized, releasing both the loading screen and input.
- A diagnostic line reports when completion occurs with active runtime queues;
  this is expected normal-world work and no longer represents unfinished load
  queues.
- The `game_loaded` key-prompt event remains bound while the load session is
  active. `Zone awaits` is therefore shown only after the warmup has finalized
  and input can respond, instead of advertising a key while input is locked.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `B3FEFA87CB9AE79F248BCA14BA7CA25081F231131F81964166BBE32E84312A04`;
  - regular DX11 PDB:
    `1D761D904C65AF2447B93BB4201E27465CA826CB3951E51B42AA3D8848D96C6B`;
  - DX11-AVX EXE:
    `8EE27125960E308FB669BB1F4412F2359B236D0B9E2E878200EC67E46ED7D9E7`;
  - DX11-AVX PDB:
    `8677B86220B4EC2556C6CBFF67801B808407BCAEC5920EBDA69C23D77CDA5532`.
- The unchanged active tuning is mirrored under
  `D:/ANTHOLOGY_DEV/addons/Anthology Performance v71 - Warmup Deadlock Fix`
  and tracked under `modpack-patches`. No unrelated addon script was touched.
- The complete pre-v71 engine binaries, symbols, source, active profile and
  failing log are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v71_pre_warmup_deadlock_fix`.
- No new game or shader-cache purge is required. Test the same save.

## 2026-08-21 - v72 measured regression rollback

### Evidence from the v71 test

- The fresh load session recorded covered world warm-up beginning with a
  5000-ms target and completing after 167 frames in 5019 ms. This was a direct
  fixed addition to every save load, so v72 changes the default and active
  `load_world_warmup_ms` value to zero. The optional mechanism remains present.
- Post-load MT profiles were render-bound: average render time was 22.63–25.11
  ms while average FrameMove time was 7.49–9.32 ms. Consequently, adding engine
  workers could not double FPS in this capture.
- Inspection of the active runtime profile found an unintended return to AO 8,
  IL 32, SSR 4, SSS 18/6, POM 36/refine 1 and terrain POM 36. v72 restores the
  previously agreed balanced values: AO 4, IL 16, SSR 2, SSS 12/4, POM 16 with
  refinement off and terrain POM 12.
- The visible periodic tails still aligned with GameThread/Lua GC work, with a
  sampled Lua GC maximum of 72.84 ms. v72 rolls the experimental v70 slicing
  defaults back to the user-accepted v66 overlap profile: step 75, 25 calls and
  a 5000-us budget. It does not attempt unsafe concurrent access to one Lua VM.
- `r__hom_dynamic` remains off. Enabling the experimental per-child path would
  repeat the existing spatial HOM work and risks restoring CPU spikes.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `134F5553554A914FA24F3731C8F110AFDE093640BA193D4B500849256C4E0AF7`;
  - regular DX11 PDB:
    `338FB1FC2A110D28331869074EA1D284D43DD755AD175450AFE2E3E6AA09DB7B`;
  - DX11-AVX EXE:
    `95008E861DD39A55886AF09C0AC9256BCBF8B5E59AD52B58C34ED89C78CCC33C`;
  - DX11-AVX PDB:
    `D0E68EA33F0AB5744F8E835004941E33F7B98D9AD88D411A6B51EE2C83F30FD8`.
- The exact active settings are mirrored as the standalone audit profile
  `D:/ANTHOLOGY_DEV/addons/Anthology Performance v72 - Regression Rollback`
  and tracked under `modpack-patches`. No addon script or MO2 ordering was
  changed.
- Complete pre-v72 binaries, symbols, relevant sources, active `user.ltx`, MO2
  list and fresh log are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v72_pre_regression_rollback`.
- No new game or shader-cache purge is required. Test the same save. The game
  was not launched during build or installation.

## 2026-08-21 - v73 adaptive frame pacing and LOD/HOM pass

### Fresh v72 evidence

- The tested save reached engine-ready in 49266 ms. Its precache contained 58
  logical frames and spent 26331 ms in FrameMove callbacks, including a
  16253-ms CLevel callback tail. This confirms that the reported visible
  ten-second settling phase was not the disabled fixed world-warmup timer.
- After engine-ready, `aaaa_script_fixes_mp.luagc_cleanup` ran two full Lua
  collections and a LuaJIT trace flush between continual timestamps 98381 and
  100148: approximately 1.77 seconds of synchronous post-load work.
- Subsequent 300-frame windows showed GameThread maxima from roughly 44 to 284
  ms and Lua GC maxima from roughly 36 to 55 ms while render averages remained
  around 7-11 ms. The remaining ticks were therefore primarily CPU/game-work
  tails in this capture, not steady GPU saturation.

### Adaptive Lua GC and diagnostics

- A standalone late-loading script unregisters only the redundant
  `on_loading_screen_key_prompt` full-GC callback. The original modpack script
  is untouched; engine load-session GC coalescing remains intact.
- Incremental Lua GC keeps Lua VM ownership on GameThread. It now waits eight
  seconds after load completion, skips already-busy frames, uses at most a
  1200-us overlap budget and six calls, and no longer forces one call after the
  renderer has already completed. Active step size is reduced from 75 to 10.
- New `[mt-frame/profile] game-breakdown` output separates scheduler,
  `seqParallel` and `seqFrameMT` averages/maxima and reports GC calls plus busy
  and post-load skips. The next real gameplay log can identify the remaining
  GameThread spike source directly.

### LOD and HOM work

- HOM occlusion results are cached only within the render-view that produced
  them. Main-camera and PiP passes use separate markers, avoiding cross-camera
  hidden-result reuse. HOM triangle-to-camera ranges are computed once before
  sorting instead of repeatedly inside its comparison function.
- LOD impostors select their best three facets in a linear eight-item pass
  rather than sorting eight pairs for every impostor each frame. Multi-batch
  indexing and zero-capacity protection were corrected as part of the same
  render path audit.
- Active SSA LOD thresholds now use a real fade interval (`56/48`) instead of
  `50/50`; `r__geometry_lod 0.85` shifts distant geometry modestly toward LOD.
  Experimental per-child dynamic HOM remains disabled because it duplicates
  established renderable-level rejection and previously increased CPU tails.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compiled and
  linked successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `33F51E4F421DE895A256AC8A824C3F09D1666456A07FD1CDC6EE37F2480C1C30`;
  - regular DX11 PDB:
    `FC79518DE23E7E048AA60B84E05D92FDF9451D96696716F06A4E0910BB454BD6`;
  - DX11-AVX EXE:
    `6758CFB5AB5C5EA0AD43D62A2152518FE7CFDBE79F3F168CF39262C26159802F`;
  - DX11-AVX PDB:
    `03379E0059E3A4736BD8A4E2789047E6ED84D94672ADFB254CCC51A79798DFB1`.
- `Anthology Performance v73 - Adaptive Frame LOD HOM` is installed and
  enabled in the active HARD MO2 profile. Identical tracked and development
  copies are kept under `modpack-patches` and `D:/ANTHOLOGY_DEV/addons`.
- The exact pre-v73 binaries, symbols, source, active settings, MO2 list and
  control log are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v73_pre_adaptive_lod_hom`. The source archive
  SHA-256 is
  `7FB0CD3A0D708B2667560F79B7E2BBEC4189C2A693731C4D5F836904C0C3EB4B`.
- No new game or shader-cache purge is required. The game was not launched.

## 2026-08-21 - v74 GC and scheduler frame-tail pass

### Fresh v73 evidence

- The fresh test log contains the v73 `[mt-frame/profile] game-breakdown`
  records, confirming that the new engine binary was active.
- Lua GC maxima repeatedly reached roughly 105-242 ms and closely matched the
  GameThread maxima. About 1650-1714 GC calls were made per 300-frame window,
  which is almost the configured six calls every frame.
- `seqParallel` reached roughly 152 ms in its worst sample. Scheduler tails were
  about 69-96 ms, with a separate 906-ms transition sample during loading.
- The renderer averages had already improved, so these recurring tails were
  CPU/GameThread work rather than a steady rendering-quality bottleneck.
- The standalone v73 Lua marker was absent. The still-running Mod Organizer had
  restored its in-memory `modlist.txt`, and the old callback consequently ran
  two full Lua collections plus a LuaJIT flush about three seconds after load.

### Tail corrections and lossless work

- Incremental GC now performs one minimal step (`1`) in an eligible frame,
  instead of six calls with step `10`. Its overlap budget is 500 us and the
  busy-frame threshold is 9 ms. The previous eight-second hiatus is removed so
  mark debt cannot accumulate and then enter an oversized LuaJIT atomic phase.
- Level load applies `pause=125` and `stepmul=100`. This starts collection before
  the heap grows as far while preserving Lua VM ownership on GameThread; the
  one Lua VM is not accessed concurrently from an unsafe worker.
- The 12-second post-load suppression of explicit full `collectgarbage()` and
  `jit.flush()` is now enforced in the engine wrappers. It no longer depends on
  MO2 successfully enabling a late-loading script addon.
- Scheduler keeps its existing time budget but checks it after every scheduled
  object instead of every eighth. Its normal batch returns from 256 to 128; no
  scheduled update frequency, game logic, NPC count or visual setting changes.
- Named `seqParallel` entries and per-object scheduler profiling now report the
  worst task every 300 frames. This makes a remaining tick attributable to a
  concrete subsystem on the next test rather than inferred from core usage.
- IX-Ray's safe DetailManager hot-loop reference caching was adapted without
  changing blade selection, density, distance, shaders or rendered output.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `31726E038A0703A4A63E6F6062249799546B2750D334F36CCC566D47103B9D8A`;
  - regular DX11 PDB:
    `729F4AA2482145859E5083961EA38CC5904892E2B2F8EBA40670F8EBD097D90F`;
  - DX11-AVX EXE:
    `E0E7813ADBC7281FCA1A0529EC4717C090F8ABF22232863DC05E62367D4CC899`;
  - DX11-AVX PDB:
    `3E1F01521B74FA50C958E6430229C144CF3E025D5545EE7A593BAABBCB18296E`.
- `Anthology Performance v74 - GC Scheduler Tail` is installed under MO2 and
  mirrored under `D:/ANTHOLOGY_DEV/addons`; the active runtime profile is also
  applied directly to `appdata/user.ltx`.
- Complete pre-v74 binaries, symbols, source, settings, MO2 list and fresh log
  are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v74_pre_gc_scheduler_tail`. The source archive
  SHA-256 is
  `1E0EE4BC06CD36416C6D07C4D4D4BF741A090E256AAB95F44CF2E472A0847285`.
- No new game or shader-cache purge is required. The game was not launched.

## 2026-08-21 - v75 v74 regression correction

### Evidence from the v74 test

- The v74 save load reached engine-ready in 65480 ms. Its payload differed from
  the v73 control (different client hash, spawn and event counts), so the entire
  difference cannot be attributed to the engine. The unseen transition was
  45947 ms versus 46781 ms in the v73 control and therefore did not regress.
- Lua GC still entered a non-preemptible LuaJIT atomic phase with step `1`: the
  fresh log recorded a 274.10-ms GC maximum and a matching 277.79-ms GameThread
  maximum. A smaller step cannot place a time limit on that atomic phase.
- Suppressing the post-load full collection prevented temporary UI/spawn Lua
  allocations from being reclaimed at the prompt. The following incremental
  cycle then processed them during visible gameplay.
- Detailed v74 profiling measured every scheduled object and every named
  `seqParallel` item with QPC calls; `seqParallel` reached about 125 items per
  frame and the scheduler processed a similar order of updates. Keeping that
  instrumentation active permanently was itself a measurable hot-path cost.
- The useful detailed capture identified `alife.update` as the recurring
  `seqParallel` tail (up to 152.77 ms). This remains a separate optimization
  target; it is not hidden by changing A-Life quality or simulation distance.

### Correction

- The verified v73 GC profile is restored: step 10, six-call cap, 1200-us
  overlap budget, 12-ms busy-frame guard and 8-second post-load delay. Lua GC
  pause and step multiplier return to LuaJIT's 200/200 baseline.
- A post-load full `collectgarbage()` is allowed again so prompt/UI/spawn
  garbage is reclaimed before ordinary play. The post-load `jit.flush()` alone
  stays suppressed for 12 seconds, preserving already compiled LuaJIT traces.
- The normal scheduler batch returns to 256 and its deadline query returns to
  once per eight objects. This restores v73 queue throughput and avoids an
  unnecessary timer query after every update.
- Per-item profiling is now controlled by the new
  `mt_frame_profile_detail` command and defaults to zero. The inexpensive broad
  300-frame breakdown remains enabled, while ordinary gameplay no longer pays
  for hundreds of detailed QPC/atomic measurements each frame.
- All visual, LOD/HOM, DetailManager, loading, addon, NPC and A-Life gameplay
  settings remain unchanged.

### Build, installation and rollback

- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `3E0ACDE90B30411E8ABD53218042A862B9B30B7A2243B0C386ADA571DEE50969`;
  - regular DX11 PDB:
    `852BF86C528DEFDC8DB94FD54D7D2266CC666D708EB4A82D9E8136DD48FEB542`;
  - DX11-AVX EXE:
    `BEC5F8C50BB75459D5503C5EB2F0AF5E3F653AA48FB88BC3E140D3F15D15E2ED`;
  - DX11-AVX PDB:
    `0686AF17C295741028359F33D2F8899BD85432D8316B452906C248AE4E53425A`.
- `Anthology Performance v75 - v74 Regression Fix` is installed under MO2 and
  mirrored under `D:/ANTHOLOGY_DEV/addons`; its runtime values are applied to
  `appdata/user.ltx`.
- Complete pre-v75 v74 binaries, symbols, source, settings, MO2 list and test
  log are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v75_pre_v74_regression_rollback`. The source
  archive SHA-256 is
  `0708AE7228082AA7BF83207E9D23F112547E56DF682C9B2604F33FD8A219A464`.
- No new game or shader-cache purge is required. The game was not launched.

## 2026-08-21 - v76 marker allocation and runtime spawn smoothing

### Evidence from the v75 test

- Steady windows were primarily renderer-bound: `seqRender` averaged roughly
  22-39 ms while `FrameMove` averaged roughly 6-11 ms. CPU-only changes cannot
  honestly double the average FPS in those scenes without a renderer-side
  optimization or a quality tradeoff.
- The visible frame-time tails were nevertheless real CPU stalls. Lua GC
  reached approximately 149-259 ms; transition-adjacent scheduler work reached
  approximately 577-854 ms; `seqParallel` reached approximately 101 ms.
- The active Catspaw HUD marker helper registered one `actor_on_update`
  callback per marker and allocated callback tables, LOS tables, screen
  vectors, copied color tables and formatted distance strings in the repeated
  marker update path. Tactic Compass additionally allocated color tables,
  vectors, a closure and a fresh seen-ID table on its periodic paths.
- Spawn Antifreeze prepared resources on its worker correctly, but
  `ProcessSpawnEvents` swapped and committed the entire published queue on one
  owner-thread frame. A worker chunk could therefore finish as a main-thread
  hitch even though preparation itself was parallel.

### Corrections

- The standalone Interaction Dot Marks override keeps the existing scan logic
  and update cadence, but reuses callback argument tables and UI vectors,
  caches the actor position across marker updates in the same millisecond,
  calculates hot-path fades without temporary tables, and only rebuilds the
  distance string when the displayed value changes.
- The standalone Tactic Compass override keeps its 30 Hz visual cadence, but
  reuses compass/marker/waveform objects, caches marker keys, reuses the config
  table, replaces the per-scan closure/table reset with a generation map, and
  calculates waveform tint/blend/fade without temporary color tables.
- Runtime spawn publication is capped at eight prepared objects per frame via
  `spawn_antifreeze_max_per_frame`. The remaining event and blueprint data stay
  queued under the existing lock. Active save/transition loading continues to
  drain the complete queue, so this pacing limit does not extend the loading
  screen.
- Lua GC remains owned by the game worker after Lua callbacks complete. It was
  not moved onto a concurrent thread because the process has one LuaJIT VM and
  the surrounding engine/addon APIs are not thread-safe.

### Build, installation and rollback

- All five overridden Lua scripts pass the Lua 5.1 parser.
- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `A7AB9233E08890B317ACFED74FA0849ABAFA75B5009BEB5602D9C9F1B6E84F43`;
  - regular DX11 PDB:
    `8EDFD0656CC81FE39D04F92300B469D71943F4B39307FB61D843D192359642B7`;
  - DX11-AVX EXE:
    `343E73E0884C3CEE1F11D24E3A69FE271D285F8DBDFBBC2621D69955F89F6F6B`;
  - DX11-AVX PDB:
    `6F63946C6D89A3E2B22C2B5F0A751D4CDC4D1B454F41C4B2F1B573D278108B25`.
- The complete v76 addon modules are mirrored under
  `D:/ANTHOLOGY_DEV/addons`; their scripts are installed into the already
  enabled, separate Dot Marks and Tactic Compass MO2 patch modules. The runtime
  spawn value is applied directly to `appdata/user.ltx`.
- Exact pre-v76 binaries, symbols, active addon modules, settings, MO2 list,
  log and source are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v76_pre_marker_spawn_smoothing`. The source
  archive SHA-256 is
  `885E093F203C433E5FC06DED3EC6BAD3C2986406DEBC3CFD3ADDA20FA8FDA661`.
- No new game or shader-cache purge is required. The game was not launched.

## 2026-08-21 - v77 v66 frame pacing with persistent CoP positions

### Fresh v76 video and profile evidence

- The latest 50.15-second, 60-FPS NVIDIA capture was decoded frame by frame.
  During continuous movement it contains repeated presentation freezes of
  approximately 50-216 ms, including adjacent stalls. This confirms severe
  frame-pacing loss despite the visibly higher v76 average FPS.
- Representative 300-frame log windows show worker waits up to 335.62 ms,
  GameThread up to 346.56 ms, `seqParallel` up to 339.65 ms and Lua GC up to
  200.32 ms. Stable render averages remain roughly 7-11 ms in the same run.
  The high-FPS renderer path is therefore retained; the recurring tick is a
  CPU/Lua/A-Life tail, not evidence that the v76 FPS work should be reverted.
- The final 6966-ms aggregate occurred while returning to the menu and dumping
  renderer resources after recording; it is excluded from gameplay diagnosis.
- The accepted v66 backup contains the exact runtime collector controls used
  by its smooth capture: step 76, 25 calls and a 5000-us overlap budget. The
  v76 profile instead used step 10, six calls, a 1200-us budget, an adaptive
  busy-frame skip and an eight-second post-load delay.

### v66 pacing restoration without the v76 FPS rollback

- GameThread still completes all Lua/script MT callbacks before touching the
  single LuaJIT VM. Incremental collection remains serialized on that owner
  worker while the main thread renders; no unsafe concurrent Lua state access
  is introduced.
- Restored the v66 collector cadence and its guaranteed first incremental step
  in the render-overlap window. The later adaptive and post-load skips are off,
  preventing mark debt from accumulating into the measured long atomic phase.
- Engine defaults, both active `user.ltx` files and the standalone
  `Anthology Performance v77 - v66 Frame Pacing` addon agree on step 76, 25
  calls, 5000 us, pause/stepmul 200/200 and no post-load delay.
- All v76 renderer, LOD/HOM, detail, marker-allocation and runtime spawn pacing
  code remains unchanged. `spawn_antifreeze_max_per_frame` remains eight.

### Persistent NPC positions and CoP smart-job fallback

- Neither A-Life companion was active in the tested MO2 profile: the old v67
  module was explicitly disabled and the v69 CoP module was absent from
  `modlist.txt`. Its placement code therefore could not affect the game.
- Stock `CSE_ALifeOnlineOfflineGroup::update()` overwrote every offline member
  with the squad centre on every scheduled update. v77 preserves each member's
  already serialized position while the squad itself is stationary.
- When the offline brain genuinely moves a squad, stock safe relocation is
  retained instead of translating a formation into an incompatible AI mesh.
  On the next online switch, exact `db.offline_objects` placement remains first
  priority and the authored CoP smart-job vertex is the fallback.
- The standalone `Anthology A-Life v77 - Persistent CoP Jobs 300m` addon keeps
  approximately 297 m online / 363 m offline hysteresis. It has no permanent
  update callback, no object scan and adds no fields to saves.

### Build, installation and rollback

- Both new Lua scripts pass the Lua 5.1 parser.
- Both `DX11|x64` and `DX11-AVX|x64` Release configurations compile and link
  successfully. Installed files match their build SHA-256 hashes:
  - regular DX11 EXE:
    `D064B783D582927821303E8AF699CDC4B0EF408903BB9208FB4510C0F3BE44A5`;
  - regular DX11 PDB:
    `5213293E3591D3274A3194C3B9B245BD167D6D50CE2D50EE6BFB38E1006BACDF`;
  - DX11-AVX EXE:
    `035D9D4171B2CC981DF0E67B5955BCE13F466EA662F124D58B6532539FA2793F`;
  - DX11-AVX PDB:
    `6884CF57F320544263895AA929436DFDC393FFB5192283F3C6C4408D4A7524E4`.
- Both v77 addons are separate working copies under
  `D:/ANTHOLOGY_DEV/addons`, consumed by MO2 through directory junctions and
  enabled at the top of the active HARD profile.
- Exact pre-v77 v76 binaries, symbols, modified sources, both user files and
  MO2 list are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v77_pre_v66_pacing_npc_persistence`.
- No new game or shader-cache purge is required. The game was not launched.

## 2026-08-21 - v78 frame-rate-normalized GC and duplicate NPC repair

### Fresh v77 regression evidence

- The latest 126.45-second NVIDIA capture contains 42 presentation freezes of
  at least 100 ms, 19 of at least 250 ms and a worst continuous freeze of
  2.303 seconds. The reported loss of smoothness is therefore reproducible.
- Across 34 gameplay profile windows, v77 performed 159127 incremental Lua GC
  calls: about 15.6 calls per frame. GameThread averaged 10.85 ms, while total
  frame time averaged 24.95 ms and the worst worker wait reached 132.95 ms.
- The v77 `do/while` also forced a GC call after rendering had already ended.
  Renderless menu windows recorded periodic 40-80 ms Lua GC waits from that
  path. This was not a renderer-quality or GPU regression.
- MO2 had both v69 and v77 300 m A-Life modules enabled. Both wrapped the same
  smart-spawn function and both expanded the online population. They are now
  disabled. The older v68 performance layers are retained because the active
  lower v76 overrides win their shared VFS paths; they do not load a second
  copy of the same script.

### Frame pacing correction without the v76 FPS rollback

- The v66 incremental step of 76 and zero post-load delay are retained so GC
  debt does not sit untouched for eight seconds after loading.
- Collector allowance is normalized by frame duration to approximately 600
  steps and 200 ms of permitted render-overlap work per second. At 100 FPS the
  limit is roughly six calls / 2 ms per frame; at 40 FPS it is roughly fifteen
  calls / 5 ms. This preserves collector throughput while no longer charging
  the v77 maximum allowance to every fast frame.
- GameThread never begins a step once render overlap has ended. The existing
  12-ms combined game-work guard remains, and LuaJIT stays serialized after
  script callbacks rather than being accessed concurrently from another VM
  owner.
- All v76 renderer, LOD/HOM, detail, marker-allocation and runtime-spawn work
  remains unchanged. No visual setting or gameplay update frequency changed.

### Persistent CoP positions without a 300 m population cost

- The active motivator binder gives `db.offline_objects[id].level_vertex_id`
  priority over `db.spawned_vertex_by_id`. Existing collapsed saves therefore
  ignored the v69/v77 smart-job fallback and still appeared at the centre.
- The new single A-Life companion leaves the current switch distance untouched.
  Unique offline vertices remain exact. Only when multiple members of one smart
  share the same collapsed vertex is that batch redirected to its already
  assigned CoP job vertices before the binder consumes the position.
- The paired engine continues preserving stationary offline member positions,
  so the repaired locations are retained after the next online/offline cycle.
  Moving squads keep stock safe relocation. There is no recurring callback,
  global NPC scan or new save field.

### Build, installation and rollback

- Both Lua scripts pass the Lua 5.1 parser. `DX11|x64` and `DX11-AVX|x64`
  compile and link successfully.
- Installed build hashes match exactly:
  - regular DX11 EXE:
    `879C33DE68C6656E1A9AD2E74832487E1309F564D6C9E2613F16655FD7EF695F`;
  - regular DX11 PDB:
    `ECA96C9DB7E5EDEDF4203184F103E83F5B95FEE8B3F964E27ACC6F2962F3FDDE`;
  - DX11-AVX EXE:
    `A9FE6F7216D616322C1C9D02ECA7D1D0E51E7E22CA81862CA30E3ACAADA8BAC3`;
  - DX11-AVX PDB:
    `78015CCF36A78CDD9CF848DA65FCB2EC87F1CECC86E1E8BFA13557E021D1F6AE`.
- `Anthology Performance v78 - Rate Normalized GC` and
  `Anthology A-Life v78 - Persistent CoP Positions` are separate working
  addons under `D:/ANTHOLOGY_DEV/addons`, linked into MO2 and enabled. The
  former v69/v77 A-Life companions and v77 frame-pacing addon are disabled.
- Exact pre-v78 binaries, symbols, settings, list, source and the fresh v77 log
  are recoverable from
  `E:/ANTHOLOGY_BACKUPS/20260821_v78_pre_v77_regression_correction`.
- No new game or shader-cache purge is required. The game was not launched.

## 2026-08-21 - v79 v78 FPS-regression correction

### Fresh v78 evidence

- The active v78 log contains eleven first-session gameplay profile windows.
  Average total time was 32.27 ms, with 21.47 ms in rendering and 8.11 ms in
  frame work. Stable windows still charged Lua GC roughly 3-5 ms per frame.
- v78 executed 23422 incremental GC calls in those windows, averaging 7.10
  calls per frame. Its frame-rate normalizer deliberately granted more calls
  and more budget after a frame slowed down. That positive feedback made a
  transient slowdown pay a larger collector cost and was the FPS regression.
- A second Jupiter-to-Zaton session confirmed the same pattern: after its
  transition window, total/frame/render time was 34.38/9.78/19.25 ms and GC
  still averaged 3.09 ms with 2950 calls per 300 frames.
- All renderer-quality settings are unchanged from the high-FPS v76 profile,
  except the pre-existing positional terrain offset. The source diff from v76
  to v78 contains no renderer code. The 19-31 ms render cost in the fresh test
  is therefore a separate scene/weather GPU ceiling, not a v78 visual change.

### Correct combination of the accepted revisions

- Restored the v76 high-FPS collector cap: incremental step 10, at most six
  calls and 1200 microseconds of render-overlap work. The 12 ms busy-frame
  guard and the requirement that rendering is still active remain in place.
- Removed the v78 target-calls/target-budget rate normalizer and both of its
  console commands. A slow frame can no longer grant GC additional work.
- Kept the post-load delay at zero instead of restoring v76's eight-second
  hiatus. The safe full collection at the load boundary remains enabled; small
  incremental work can then continue immediately, avoiding delayed debt.
- Kept v78 A-Life position persistence and its standalone repair addon exactly
  as-is. No renderer, PiP, graphical setting or unrelated gameplay script was
  modified.

### Build, installation and rollback

- The v79 Lua companion passes the Lua 5.1 parser. Both `DX11|x64` and
  `DX11-AVX|x64` compile and link successfully.
- Installed build hashes match exactly:
  - regular DX11 EXE:
    `E132A02BDCBD6CE48A14B1E9167E801358784205800589CD2A94F9E1F8248646`;
  - regular DX11 PDB:
    `5AF0191EC39137657425C2F0BB62CDB4087974B91F34B38AA3F26F339A11A3A9`;
  - DX11-AVX EXE:
    `F98AEE213E6F756A81DB3D9E8DF380ACD61879121D39D1F3ADD4DB9D47568F56`;
  - DX11-AVX PDB:
    `93E339AEA22C023D29976843A9E2DFB0232FF5EB3ADA078E21BEA783830F99CF`.
- `Anthology Performance v79 - Continuous Small GC` is a separate working
  addon under `D:/ANTHOLOGY_DEV/addons`, linked and enabled in the active HARD
  profile. The v78 GC addon is disabled; v78 A-Life remains enabled.
- Exact pre-v79 binaries, symbols, settings, active mod list, source snapshot,
  addons and the fresh v78 log are backed up at
  `E:/ANTHOLOGY_BACKUPS/20260821_v79_pre_v78_fps_regression`.
- No new game or shader-cache purge is required. MO2 was closed cleanly and
  the game was not launched.

## 2026-08-21 - v80 original CoP online NPC placement

### Why v78 still allowed centre spawns

- The active `z_npc_footsteps.script` calls
  `smart_terrain.setup_gulag_and_logic_on_spawn` before it consumes the saved
  offline vertex. The active `smart_terrain_ex.script` may clear that table
  while changing/starting an assigned job.
- Original Call of Pripyat resolves a spawned vertex, saved offline vertex or
  assigned smart-job position before setting up the new online logic. The
  modpack inversion meant that v78 sometimes had no vertex left to repair; its
  startup marker proved wrapper installation, not successful placement.

### Isolated compatibility correction

- A late wrapper around the already active motivator `net_spawn` captures the
  offline vertex before the original binder and restores it before the first
  rendered gameplay frame. It does not replace Exo/footstep binder logic.
- A unique saved position has priority. A duplicate squad-centre batch is
  redirected to the already assigned smart job. NPCs whose squad is still in
  `arriving_npc` are not teleported to a destination job.
- The correction has no recurring update, global NPC scan, wider online range,
  renderer change or save field. v78's Lua wrapper is disabled, while its paired
  engine-side stationary-member preservation remains in the v79 binaries.

### Installation and rollback

- `Anthology A-Life v80 - CoP Online Placement` passes the Lua 5.1 parser, is
  mirrored under `D:/ANTHOLOGY_DEV/addons`, linked into MO2 and enabled. The
  v78 A-Life Lua addon is disabled to prevent two wrappers of the same path.
- Mocked binder-path checks pass for a unique offline vertex, a collapsed
  duplicate batch and an arriving squad which must not be job-teleported.
- The DX11 and DX11-AVX v79 binaries are unchanged. No rebuild is required for
  this isolated Lua compatibility layer.
- Exact pre-v80 active mod list, v78/v79 addon copies and runtime log are backed
  up at `E:/ANTHOLOGY_BACKUPS/20260821_v80_pre_cop_online_placement`.
- A new game and shader-cache purge are not required. The game was not launched.
