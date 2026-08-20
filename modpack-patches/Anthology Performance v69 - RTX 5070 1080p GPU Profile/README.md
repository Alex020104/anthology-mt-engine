# Anthology Performance v69 - RTX 5070 1080p GPU Profile

Applied to the active `appdata/user.ltx` and stored here as an independently
auditable profile. This is not a gameplay-script override: shader quality
defines must be present before renderer resources are created.

The profile returns the most expensive SSFX sample counts toward the engine's
balanced defaults while retaining indirect light, ambient occlusion, screen
space reflections, directional/omni screen-space shadows and parallax:

- enables the May 2026 Monolith dynamic-HOM culling path for occluded world
  objects (`r__hom_dynamic on`); HUD/PiP-specific queues are excluded
- IL samples: 32 -> 16
- AO samples: 8 -> 4
- SSR quality: 4 -> 2
- SSS directional/omni samples: 18/6 -> 12/4
- POM samples/refinement: 36/on -> 16/off
- terrain POM samples: 36 -> 12

It deliberately leaves texture quality, resolution, sun quality, volumetric
lighting, water reflections, vegetation density/radius, PiP and SSS temporal
stability untouched. Dynamic HOM can be disabled independently with
`r__hom_dynamic off` for an immediate A/B comparison.

The exact former `user.ltx` is in the paired v69 backup on drive E.
