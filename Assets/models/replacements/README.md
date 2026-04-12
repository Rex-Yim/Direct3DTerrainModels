# Static Asset Replacement Pipeline

The app now supports static assets in Wavefront OBJ/MTL as a practical replacement for broken legacy `.x` files.

Priority targets:
- `model_jeep`
- `model_crate`
- `model_boulder`

The runtime lookup order for each target is:
1. `Assets/models/replacements/<name>/<name>.obj`
2. `Assets/models/replacements/<name>.obj`
3. `Assets/models/<name>.obj`
4. `Assets/models/<name>.x` (legacy fallback)
5. Built-in primitive fallback mesh (last resort)

## Expected file layout

Option A (recommended):

```text
Assets/models/replacements/model_jeep/model_jeep.obj
Assets/models/replacements/model_jeep/model_jeep.mtl
Assets/models/replacements/model_jeep/<textures...>

Assets/models/replacements/model_crate/model_crate.obj
Assets/models/replacements/model_crate/model_crate.mtl
Assets/models/replacements/model_crate/<textures...>

Assets/models/replacements/model_boulder/model_boulder.obj
Assets/models/replacements/model_boulder/model_boulder.mtl
Assets/models/replacements/model_boulder/<textures...>
```

Option B (single file pair per model in the replacements root):

```text
Assets/models/replacements/model_jeep.obj
Assets/models/replacements/model_jeep.mtl
Assets/models/replacements/model_crate.obj
Assets/models/replacements/model_crate.mtl
Assets/models/replacements/model_boulder.obj
Assets/models/replacements/model_boulder.mtl
```

## Conversion guidance

1. Open original source model in Blender/Maya.
2. Apply transforms and triangulate mesh.
3. Export as OBJ with:
   - positions
   - normals
   - UVs
   - generated MTL
4. Keep texture paths relative (e.g. `map_Kd diffuse.png`) and copy texture files beside the OBJ/MTL.
5. Ensure unit scale is consistent with scene gameplay dimensions.

## Notes

- The OBJ loader supports:
  - `v`, `vt`, `vn`, `f`
  - `usemtl`
  - `mtllib`
  - material diffuse/ambient color (`Kd`, `Ka`)
  - diffuse texture (`map_Kd`)
- Faces with more than 3 vertices are triangulated at load time.
- If normals are missing, the loader computes them.
- Windmill remains on the existing `.x` animated path (`AnimatedModel`).
