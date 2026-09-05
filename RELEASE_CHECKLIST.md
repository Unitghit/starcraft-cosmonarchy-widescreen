# Release checklist

## v0.5.2 preparation, 2026-09-05

- The user tested zoom-adjusted panning and reported that it seems good.
- Live inspection confirmed all five camera-input calls use the new adapters.
  Native minimap, scripted, portrait, and control-group centering calls are not
  patched by this change.
- Fractional-motion and x86 adapter tests are included in the release gate.
- The full v0.5.2 release build passed: motion/ABI, zoom/input, panning geometry,
  HUD sizing, graphics backends, configurator, and release-cleanliness checks.
- Runtime diagnostics are compiled out. The five-file ZIP allowlist excludes
  AI repair, map reveal, captures, test tools, and game files. ZIP entry bytes,
  standalone checksums, and EXE version 0.5.2.0 were verified.
- The rebuilt renderer's executable code section matches the user-tested
  candidate. No further runtime implementation changes were made for packaging.
- Local bundle: `artifacts/release/StarCraft-Cosmonarchy-Widescreen-v0.5.2.zip`.
  ZIP SHA256: `A4D7CE468BC53C7C82FBB21D179888EBE104BDF39A9ECDB57DACC95434CA4AC4`.
  EXE SHA256: `5012D862397BE062538486E494D961DFDF43CAB2793A359C404FC4600BDCDDB0`.
- The user approved committing, pushing, and publishing this verified v0.5.2
  bundle. No new Linux/macOS interactive coverage is claimed.

## v0.5.1 verification, 2026-09-05

- The user confirmed that blocked regions could no longer be found after
  repeated zooming and order clicks on the installed hotfix candidate.
- Native routing tests cover 1,329,132 points and the captured 350% zoom case.
  Existing geometry, zoom, HUD, selection ABI and graphics tests pass.
- Runtime diagnostics remain compiled out. External capture tools are stopped
  and their local artifacts are excluded from the build and source control.
- The full release build gate passes, including panning, GUI, OpenGL/D3D9/GDI,
  diagnostic exclusion and the five-file release bundle allowlist. Historical platform
  coverage does not imply new Linux/macOS interactive testing for this hotfix.

## v0.5.0 verification, 2026-09-04

- Release renderer and configurator builds pass. Existing upstream C++/FreeType
  warnings remain; the configurator publish has no warnings.
- Geometry, zoom, selection bounds, HUD sizing, panning, GUI, OpenGL and D3D9
  regressions pass. Both final bug fixes were confirmed by the user in-game.
- Runtime diagnostics are compiled out. The ZIP allowlist excludes AI repair,
  map reveal, test executables, captures and game files.
- Versioned ZIP and standalone EXE hashes are recorded in `SHA256SUMS.txt`.
- Historical interactive coverage is recorded in the system map. The generic
  checklist below remains a reusable checklist, not a claim that every scenario
  was rerun for this package. macOS remains untested.

## Full regression checklist

- [ ] Renderer Release/Win32 rebuild succeeds.
- [ ] Configurator Release publish succeeds without warnings.
- [ ] Offline geometry matrix passes every preset and representative custom
      aspect ratios, including the 3840x2160 maximum.
- [ ] Clean-install **Save**, **Save & Play**, and **Restore Original** pass.
- [ ] `--validate`, representative `--apply-profile`, `--apply-custom`, and
      `--restore` smoke commands pass in an isolated compatible installation.
- [ ] 1280x720 full interactive regression checklist passes.
- [ ] At least one non-preset custom resolution starts and renders.
- [ ] 3840x2160 reaches a match; long load-time warning remains visible.
- [ ] Stable GPTP SHA-256 remains
      `CC6BF422B4DC6174EC6B002ACAE12A826D61CBF144661FE0C4F9E3687664BB99`.
- [ ] Source tree contains no game executables, QDPs, maps, archives, memory
      dumps, logs, screenshots, or local absolute paths.
- [ ] Version, changelog, release notes, executable, and SHA-256 checksum agree.
- [ ] GitHub release includes the executable and checksum; GitHub-generated
      source archives remain enabled.
