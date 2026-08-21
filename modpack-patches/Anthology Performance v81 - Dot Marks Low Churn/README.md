# Anthology Performance v81 - Dot Marks Low Churn

Standalone MO2 override for the active Interaction Dot Marks stack.

- Keeps the accepted v76 marker caches, staggered scans and per-marker scratch
  state.
- Uses the paired engine's unsorted spatial scan. The original sorted binding is
  used automatically when an older executable is launched.
- Reuses hot UI position, size and direction vectors.
- Clears the pickup index in place while moving instead of allocating two new
  tables every frame.
- Keeps targeting, scan radii, MCM values, marker appearance and update cadence
  unchanged.

Rollback: disable this addon in MO2. The lower Interaction Dot Marks providers
are not modified.
