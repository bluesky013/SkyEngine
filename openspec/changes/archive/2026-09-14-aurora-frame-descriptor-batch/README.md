# aurora-frame-descriptor-batch

Frame-scoped DescriptorBatch for cross-set updates, with two lifetime modes: transient (re-allocate per update) for Global/Pass, and cached (stable descriptor + per-frame pack buffer) for Batch tier
