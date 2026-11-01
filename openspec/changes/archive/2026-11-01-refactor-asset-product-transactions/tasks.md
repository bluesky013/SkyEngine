> Status — 2026-11-01: superseded；现行 owner 为 `simplify-asset-product-storage`。本 change 仅保留中间快照历史，无独立实施任务。
> 两份通用 delta 已与主 spec 一致；旧 ProductGroup/世代快照及其未执行验收不得重新作为当前契约同步。

> Successor: `simplify-asset-product-storage` replaces the product-group/generation implementation below. Historical validation is retained as evidence of that intermediate snapshot, not a current storage or compatibility guarantee.

## Extraction

- [x] 1. Identify Core/Framework transaction, snapshot, generated-catalog and cook-completion changes.
- [x] 2. Keep animation schema, UI/package integration and metadata/code-lifetime policy in their owning changes; current code residency is provided by the loader cleanup.
- [x] 3. Include generic main specifications and transaction documentation with the implementation.

## Validation

- [x] 4. Build and run FrameworkTest on the isolated intermediate snapshot.
- [x] 5. Check strict OpenSpec validation, patch application and equality with the final working tree after recombination.
- Superseded 6: the non-Windows native atomic test was not executed for this intermediate ProductGroup snapshot. Current flat-product native replacement/error coverage belongs to `simplify-asset-product-storage`; retain this historical limit without reopening the withdrawn storage model.
