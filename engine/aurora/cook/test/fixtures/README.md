# Skeletal mesh cook fixtures

`two-bindings.gltf` uses one triangle geometry on two nodes with separate skins. Both mesh nodes have translation `(4, 0, 0)` and scale `(-2, 3, 1)`. Rig B is translated four units in Y. Each rig includes an unweighted helper between root and tip. The two weighted slots use weights `0.25 / 0.75`; their original inverse binds differ. After normalization the geometry can share one Mesh while retaining two Skin and two Skeleton products.

Moving the helper two units in X moves each weighted vertex `1.5` model-space units in X. Tests independently apply an Actor transform and check the resulting CPU LBS positions against transformed per-slot bounds. A separate numeric test checks linear tangent transformation, inverse-transpose normals, mirrored winding and preservation of material ranges during optimization.

`four-influences.gltf` and `five-influences.gltf` exercise raw accessors before Assimp can discard or reduce influences. The remaining fixtures isolate negative/zero weights, a singular mesh transform, reference mismatch and local shear. Buffers are embedded so the fixtures can be loaded directly from memory without filesystem callbacks.
