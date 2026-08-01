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
