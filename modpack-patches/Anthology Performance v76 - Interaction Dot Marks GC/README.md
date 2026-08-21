# Anthology Performance v76 - Interaction Dot Marks GC

Standalone patch for the active Interaction Dot Marks stack.

- Retains the existing movement-aware pickup scan fix.
- Reuses per-marker callback argument tables and screen vectors.
- Reuses the actor position across marker updates in the same millisecond.
- Calculates fade colors without temporary Lua tables.
- Rebuilds distance text only when the displayed tenth of a metre changes.

The original addon is not edited in place in the tracked source. This complete
module is intended to override its two scripts at higher MO2 priority.
