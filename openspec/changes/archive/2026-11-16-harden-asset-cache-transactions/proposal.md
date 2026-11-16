## Why

Asset scans repeatedly reload manifests, and cache readers borrow mutable storage across unlocked calls. The reviewed follow-up must first establish safe snapshot, parsing and commit boundaries before reducing lock scope or optimizing editor consumers.

## What Changes

- Make manifest and product-index readers hold immutable snapshots; separate authoritative reads from publication.
- Preserve valid entries from damaged directories for read-only diagnostics, never seed or rewrite damaged identities.
- Batch scans, validate mutation destinations and preserve explicit failure/recovery states.
- Reconcile AllowCook source precedence with the main path-resolution requirement.
- Implement the approved tooltip/settings, shared payload budget and command-incarnation follow-ups in their editor/renderer domains.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `assets`: snapshot lifetimes, degraded scanning, mutation boundaries and path precedence.
- `editor-shell`: interaction scopes and atomic settings targets.
- `aurora-renderer`: shared payload accounting and late preview defaults.
- `editor-core`: document/object identity validation for commands.

## Impact

Framework asset indexes, database, manager, builders and consumers; Framework and editor regression tests. No directory relocation, new target, commit or archive is included. This change owns new remediation; `fix-asset-reference-boundaries` remains evidence for its delivered work, not acceptance evidence for this follow-up.
