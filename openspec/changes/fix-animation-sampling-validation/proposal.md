## Why

Malformed rig/channel data, stale unanimated pose values and inconsistent player controls must be handled before the skeletal workflow binds runtime instances. These fixes belong to the existing renderer-independent Animation library and can be validated separately.

## What Changes

- Validate skeleton topology, reference transforms, channel keys and clip duration with explicit status results.
- Reset sampling destinations to the reference pose before applying partial channels.
- Preserve source frame counts and align virtual-graph and lowered-plan playback controls.
- Keep the existing Animation target and directory layout.

## Capabilities

### Modified Capabilities

- `animation`: validated runtime construction, sampling and playback.

## Impact

Only the existing Animation algorithms and their tests. Asset codecs, model import, World instances, renderer skinning and Graph editing remain in `add-skeletal-animation-workflow`; their acceptance is not implied by this batch. This change extracts already implemented work for review and includes its main specification updates. Commit and archive require user confirmation.
