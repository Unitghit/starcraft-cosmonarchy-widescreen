# Positional audio

## Symptom

Units visible beyond the legacy 640x400 upper-left viewport could fight without
their local combat sounds being audible. Stereo direction was also centered on
the old 320-pixel midpoint instead of the expanded battlefield midpoint.

## Ownership chain

1. Cosmonarchy's stable GPTP `AudioSystem` stores a sound's world position.
2. `PlaySoundSettings::calculate_volume_pan` evaluates that position when the
   sound starts and again while it remains active.
3. GPTP delegates volume to StarCraft `0x0048E8D0` and pan to `0x0048E850`.
4. Those native helpers read the outer camera globals at `0x0062848C/A8`.

The expanded compositor restores the outer camera after every private render
pass, so the camera source is already correct. The defect was the fixed
viewport geometry inside the two audio helpers.

## Runtime geometry patch

`PatchPositionalAudioViewport` preflights the complete relevant opcode runs and
five geometry operands before changing anything:

| Operand | Native | Derived value |
|---|---:|---:|
| pan center in 32-pixel tiles | 10 | `game_width / 64` |
| right volume edge (two operands) | 640 | `game_width` |
| lower volume edge | 400 | `game_height` |
| negative lower-edge displacement | -400 | `-game_height` |

The stock attenuation thresholds and stereo-pan curve remain untouched. A
sound within the expanded battlefield therefore receives the same on-screen
volume treatment as a sound in the native viewport, while its direction is
measured from the expanded horizontal center. Sounds beyond the expanded view
continue to attenuate normally.

## Coordinate invariants

- Sound positions and camera coordinates are world pixels.
- Audio viewport width and height use internal battlefield geometry.
- External presentation magnification must never affect audio calculations.
- Fog/visibility filtering, sound flags, minimum volume, duplicate suppression,
  and DirectSound volume ranges remain owned by stable GPTP.
- Private compositor cameras must be restored before simulation/audio updates.

## Validation

Test equivalent combat sounds at the left edge, center, right edge, and lower
edge. Repeat while scrolling and while a looping or sustained sound is already
playing, because GPTP recalculates active sound volume and pan continuously.
