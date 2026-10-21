## Why

Pass C++ compilation currently waits for ShaderHeaderTool and Slang reflection to produce its types and binding schema. Shared pass declarations should be compilable by C++ and shaders independently, with reflection validating their agreement rather than generating the C++ source.

## What Changes

- Replace the ten Slang-owned declaration files with shared `.h` declarations owned by the same Global/Pass/Batch domains.
- Expand one field/resource list into C++ types, typed slots and layout metadata, or shader declarations. Keep shader reflection validation and existing native binding rules.
- Remove the production ShaderHeaders dependency and obsolete mirror generator; retain the independent variant-header tool without Slang linkage.
- Keep source embedding as a CMake text operation and update tests, active design documents and affected specifications.

## Impact

Existing Aurora Core header helpers, Pipeline declarations/consumers, shader utilities and tests. No new library, runtime module, backend policy or rendering tier. Existing targets keep their names and ownership; their obsolete codegen edges are removed.

## Stage closeout (2026-10-21)

Implementation and Windows validation are complete for the built-in pipeline/shared-header scope; see tasks.md for evidence and remaining native Metal validation limits. General user/plugin pipeline ownership, namespaces, resource bundles and runtime/cook integration are deferred to the separate proposal `refactor-aurora-pipeline-resource-ownership`. This checkpoint does not claim those plugin capabilities are implemented. No archive or commit has been performed.
