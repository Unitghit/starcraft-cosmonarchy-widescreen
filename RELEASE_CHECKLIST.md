# Release checklist

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
