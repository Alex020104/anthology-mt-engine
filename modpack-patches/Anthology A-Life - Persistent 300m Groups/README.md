# Anthology A-Life - Persistent 300m Groups

Runtime companion for the paired Anthology engine build.

- Sets the real online threshold to approximately 297 m.
- Uses an approximately 363 m offline threshold to prevent boundary thrashing.
- Installs no per-frame callback and writes no new save data.
- The paired engine preserves each offline squad member's relative position
  after that squad has been online once, instead of collapsing every member to
  the squad centre on every offline update.

Existing saves are supported. A squad already collapsed in an old save will
spread through its normal smart-terrain jobs after the first online cycle; its
formation is then preserved on later cycles.

Rollback: disable this MO2 mod and restore the v66 known-good engine tag.
