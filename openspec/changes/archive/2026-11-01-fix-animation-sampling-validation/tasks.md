> Status (2026-11-01): **ready-to-archive**；owner：`fix-animation-sampling-validation`。已交付范围和既有验证齐备，主 spec 同步与 archive 等待用户确认；不扩大已记录的平台验收边界。

## Extraction

- [x] 1. Isolate existing Animation algorithm changes and validation tests.
- [x] 2. Transfer runtime duration, control and reference-pose requirements with their implementation.
- [x] 3. Retain source/asset, World and editor integration acceptance in the skeletal workflow.

## Validation

- [x] 4. Build and run AnimationTest on the isolated intermediate snapshot.
- [x] 5. Validate OpenSpec and verify sequential patch reconstruction without changing the original index.
