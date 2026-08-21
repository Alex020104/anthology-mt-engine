# Anthology Performance v84 - V81 FPS GC Restore

Standalone runtime companion for the V84 engine candidate.

It restores the measured V81 Lua GC feed rate (76 KB steps, up to ten calls
inside the existing 1200 us render-overlap allowance), removes the V83 duplicate
pre-atomic remark pass and keeps the low-risk finalized-userdata relink.

The paired engine build also fixes the long-standing A-Life switch-budget unit
conversion. A configured 900 us budget with a 0.1 monster share is now 0.81 ms,
not almost 900 seconds. This preserves object counts, switch order, online
distance and NPC placement while preventing one periodic `alife.update` batch
from occupying the game worker for 50-219 ms.

Per-item `seqParallel` attribution is disabled in the test profile because the
cause is now known and timing every item adds avoidable QPC/atomic overhead. No
renderer, loading, LOD/HOM, PiP, save or addon-script behaviour is changed.
