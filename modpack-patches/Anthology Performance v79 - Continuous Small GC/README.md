# Anthology Performance v79 - Continuous Small GC

Runtime companion for the paired v79 engine build.

- Restores the small v76 collector slice: step 10, at most six calls and
  1200 microseconds during render overlap.
- Removes v78 frame-rate normalization. A slow frame no longer grants the
  collector extra work and cannot create a self-amplifying FPS drop.
- Removes v76's eight-second post-load pause. The full load-boundary collection
  remains enabled, then small incremental work may continue immediately.
- Keeps the 12 ms busy-frame guard and never starts an incremental step after
  renderer overlap has ended.

No renderer setting, gameplay callback, save field or A-Life rule is changed.
The separate v78 persistent-position addon remains compatible and active.
A new game and shader-cache purge are not required.
