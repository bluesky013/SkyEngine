## 1. Transform / opacity core

- [x] 1.1 Add `UI2DTransform` (affine 2x3) to `UIRect.h`
- [x] 1.2 Add `UITransform` (translation/rotation/scale/pivot) with `ToMatrix` and `IsIdentity`
- [x] 1.3 Add transform + opacity stacks to `UIPaintContext`; transform positions and scale alpha in `AddQuad`
- [x] 1.4 Add `UIElement` transform/opacity and push/pop them in `Paint`

## 2. Tests

- [x] 2.1 Tests: translation shifts vertices, parent transform affects child, opacity scales alpha, parent opacity cascades
- [x] 2.2 Build and run the suite green
