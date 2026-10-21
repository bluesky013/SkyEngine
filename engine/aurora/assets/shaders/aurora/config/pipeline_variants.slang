// Global pipeline variant layout: single source of truth for the pipeline
// variant keys (set by PipelinePass, shared across all shaders). Loaded by the
// pipeline module at init and validated against the reserved bit budget.

// ===== @variant =====
// @reserved 16
// @source pipeline
//   SHADOWS : bool = 0
//   IBL     : bool = 0
// ===================
