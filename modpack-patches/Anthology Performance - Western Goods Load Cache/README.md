# Anthology Performance - Western Goods Load Cache

Standalone MO2 patch for the current Western Goods configuration.

- Reuses the UI texture atlas already parsed by the engine instead of parsing
  all `textures_descr` XML files once per readable on every save load.
- Parses only the one current dialog file that contains `<rand_text>` and
  keeps its DXML result for the process lifetime.
- Does not change readable contents, page lookup, dialog text, saves or MCM.

Requires the matching Anthology engine build that exports `ui_texture_ids`.
The script retains the original XML parser as a compatibility fallback.
