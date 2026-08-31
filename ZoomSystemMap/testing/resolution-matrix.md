# Resolution matrix

## Current geometry model

The offline verifier currently exercises:

| Output | Battlefield | Coverage status | Interactive status |
|---:|---:|---|---|
| 640x480 | 640x400 | Offline + universal build pass | Experimental, untested |
| 800x600 | 800x520 | Offline + universal build pass | Experimental, untested |
| 960x720 | 960x640 | Offline + universal build pass | Experimental, untested |
| 1280x960 | 1280x880 | Offline + universal build pass | Experimental, untested |
| 960x540 | 960x460 | Offline + universal build pass | Historical prototypes unstable; current architecture untested |
| 1024x576 | 1024x496 | Offline + universal build pass | Untested |
| 1280x720 | 1280x640 | Offline pass | Fully user-tested working target |
| 1366x768 | 1366x688 | Offline pass | Untested |
| 1600x900 | 1600x820 | Offline + universal build pass | Untested |
| 1920x1080 | 1920x1000 | Offline + universal build pass | User-tested before runtime unification |
| 2560x1440 | 2560x1360 | Offline + universal build pass | Untested |
| 3840x2160 | 3840x2080 | Universal maximum-bound build pass | Untested stress target |

“Offline pass” means the derived tile grid covers the battlefield exactly,
each source crop stays within native safe limits, and HUD/popup placement fits.
It does not prove runtime allocation, instruction operand width, performance,
or interactive correctness.

## Runtime configuration invariants

- Read and validate internal dimensions before installing any geometry patch.
- Keep maximum backing storage fixed at 3840x2160 and clear/copy only the active
  frame extent.
- Recalculate every dependent rectangle through `resolution::Configure()`.
- Reject internal dimensions outside 640x480..3840x2160.
- Keep internal geometry independent of external presentation dimensions.
- Verify cnc-ddraw behavior, DPI scaling, and physical pitch per target.
- Run the full regression checklist at representative 4:3, 16:9, 16:10, and
  ultrawide resolutions.
- Add a safe fallback to 1280x720 if configuration validation or patch
  preflight fails before a match starts.

## Proposed runtime test tiers

1. Baseline: 640x480 and 1280x720.
2. Common: 1366x768, 1600x900, and 1920x1080.
3. Aspect coverage: 800x600 and a 16:10 target.
4. Stress: 2560x1440, 3840x2160, and one ultrawide custom target.
5. Map-edge coverage: small and maximum-size maps at every tier.

Update this table only after recording whether the result was geometry-only,
automated runtime, or interactive user verification.
