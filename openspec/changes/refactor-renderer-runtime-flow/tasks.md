## Implementation

- [x] Trace and document the actual command, ECS freeze, system dependency, publication and GPU handoff chain.
- [x] Extract packet synchronization and prepared-frame handoff stages without moving ECS algorithms into Runtime.
- [x] Extract GPU progress and graph stages with centralized failure cleanup and correct handling of upload preparation that admits no batch.
- [x] Review the refactor against the saved baseline for ordering, lifetime and module-boundary preservation.
- [x] Build affected production/runtime targets and run existing CPU ECS and native GPU regression suites.
- [x] Record verification and prepare separate concrete commit messages/file lists for user approval; do not commit or archive automatically.
