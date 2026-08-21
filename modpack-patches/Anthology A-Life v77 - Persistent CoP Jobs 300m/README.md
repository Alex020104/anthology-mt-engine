# Anthology A-Life v77 - Persistent CoP Jobs 300m

Standalone runtime companion for the paired v77 engine build.

- Uses approximately 297 m online and 363 m offline thresholds.
- The engine now retains each stationary offline member's existing serialized
  position instead of overwriting the whole squad with its centre every update.
- If a squad genuinely travels in offline simulation, stock safe relocation is
  retained; when it returns online, the assigned Call of Pripyat smart-job
  vertex is used as a fallback for an authored camp/guard/work position.
- Existing saves are supported and no new persistent fields are added.
- There is no recurring scan or `actor_on_update` callback.

An already collapsed squad must come online once so its smart jobs can spread
it. Its resulting positions are preserved on later offline/online cycles.

Rollback: disable this addon and restore the paired v76 binaries from
`E:/ANTHOLOGY_BACKUPS/20260821_v77_pre_v66_pacing_npc_persistence`.
