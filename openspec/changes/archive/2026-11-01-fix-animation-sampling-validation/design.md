## Runtime boundary

Skeleton and clip construction validate before exposing runtime objects. Invalid topology, names, transforms, key arrays, sample times and playback controls return AnimationStatus. Skeleton layout signatures describe stable topology and reference data; construction does not register metadata or depend on asset handlers.

Every sample starts with the selected skeleton's local reference transforms. Channels replace only the animated components, so reused pose storage cannot retain values from a previous partial clip. Unknown bone channels fail validation rather than disappearing silently.

Explicit source frame counts survive reduced key arrays and must cover the last key. Without explicit length, duration is derived from the last key and frame rate. Playback and seek use the same time source in virtual and lowered-plan evaluation; paused initialization stays paused.

## Split ownership

This is a prerequisite extracted from `add-skeletal-animation-workflow`, with no target or directory changes. That workflow still owns preserving frame counts through asset codecs, validating imported data, creating per-World instances and renderer/editor integration. The standalone algorithm tests do not prove those integration requirements.
