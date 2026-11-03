# Gizmo Customize and deformers (custom 0.10.8)

Press **W** (Move), **E** (Scale) or **R** (Rotate). Click the **gear above the Gizmo**. The same menu opens from **Customize / Deformers** in the Gizmo panel, or F3 search.

The Transform Type menu follows the reference's primitive row and four-column operator order. The primitive buttons **append a new SubTool** to preserve your sculpt; they do not replace it. This differs deliberately from ZBrush's replace-unmasked-SubTool default. PolyCylinder/Cylinder and PolySphere/Sphere currently use the same family of generators. Resolution controls remain in Tool > Initialize; there is no live parametric-primitive cone editor.

## Preview workflow

Choose a deformer. Change its axis, amount, side or other controls in the **Gizmo Deformer** panel. An orange cage shows its bounds. Drag the orange cone/dot to change the amount. Cage deformers have eight draggable corners; Bend Curve has four draggable Bezier control points. Dragging is in the screen plane. Numeric displacement controls provide depth adjustment.

Every preview is recomputed from the starting snapshot. **Reset** restores neutral settings. **Cancel** (or Ctrl+Z) restores the starting mesh without adding history. **Accept** commits one undo step. **Q** accepts then returns to Draw. Changing SubTools cancels the old SubTool preview. A command commits the preview first, so saving or applying another tool cannot silently discard it. Gizmo modifiers act on the active SubTool, even with All SubTools enabled.

| Operator | Behavior and controls |
| --- | --- |
| Bend Arc | Circular bend, axis, signed degrees, side anchor. |
| Bend Curve | Four-point cubic Bezier displacement curve; per-point X/Y/Z offsets. |
| Deformer / Hard / Soft | Eight-corner lattice. Trilinear / locally concentrated linear / smoothstep weights. |
| Flatten | Pushes one side's vertices onto a plane; no faces removed. |
| Inflate | Normal inflation increasing from 0 at the fixed side to full at the working side. |
| Offset / Rotate / Scale | Translation / rotation / axis scaling in the Gizmo frame. |
| Skew | Shear increasing toward the working side. |
| Smooth / Smooth All | Side-weighted / whole-selection polygon-edge smoothing; optional volume preservation. |
| Stretch | Scales along the axis while keeping the opposite side anchored. |
| Taper | Scales cross-section progressively; adjustable exponent. |
| Twist | Progressive axis rotation; signed degrees. |
| Slice / Multi Slice | One / 1-12 parallel plane cuts preserving both sides and creating PolyGroups. Amount moves the planes. Requires a visible, unmasked mesh; two-million-triangle preview guard. Transition faces may be triangles. UV coordinates are retained; topology cuts clear UV editing seam/pin marks. No strip inflation, width, or automatic creasing yet. |
| Subdivide | Existing one-level smooth subdivision, preserving quads where available; one Undo step. |
| Remesh By DynaMesh | Existing quad surface-nets DynaMesh, current Tool resolution/settings. |
| Remesh By Quad Remesh | SculptForge's own Quad Remesh and its Tool settings; not ZRemesher. |
| Remesh By Decimation | Existing decimation, retaining half the triangles. |

Deformers honor soft masks and fully hidden vertices. Nonlinear modifiers move the evaluated surface and its sculpt-layer base together; existing layer offsets remain unchanged. They are not recorded into the active sculpt layer. Micro, subdivision stacks and armatures must be baked/converted first. The eight-corner cages and four-point curve are initial implementations; variable cage division, selected-point rotation/scaling and ZBrush's full cone vocabulary are not implemented. Deformers do not inherit stroke radial/mirror symmetry; the cage itself can be adjusted symmetrically by numeric entries.

**Bevel, Crease, Extender, Project Primitive and Remesh By Union** are visibly disabled with an explanation. They are not mapped to an unrelated command.

## Operators above the Gizmo

- **Gear:** Customize.
- **Pin / Sticky:** keep the pivot at its starting position after a transform.
- **Center:** center the pivot on visible, unmasked bounds without resetting orientation.
- **Home / Mesh to Axis:** translate the selection to the world origin; Alt or unlock moves only the pivot.
- **Reset orientation:** turn the selection from the current Gizmo frame back to world axes; Alt or unlock resets only the axes. This uses the current frame, not a stored import orientation.
- **Lock:** locked transforms the model; unlocked moves or rotates only the pivot. Alt temporarily unlocks.
- **Layers:** transform all visible SubTools; deformers still affect only the active one.

The pre-existing mask/falloff, transpose-line, surface-pivot and numeric transform controls remain in the normal Gizmo panel.

## Upstream merge

This custom build merges supplied upstream 0.10.7 against upstream 0.10.1, retaining custom 0.10.5 changes. New packs/macros, PBR viewport/fill-layer systems, UV editing, selection-set tools, radial/local symmetry, deformations, tablet/navigation fixes, alpha fixes and optional themes remain included. Classic with the extended sculpt shelves and ZBrush-style input remains the custom default. Upstream displays ClayBuildup as Clay Block and DamStandard as Sharp Crease; their custom behavior is retained. Optional layouts can be selected in Preferences.

## Reference

The workflow and behavior categories were checked against Maxon's official ZBrush documentation (2 November 2026):

- [Gizmo operators](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/modeling-basics/gizmo-3d/operators/operators.html)
- [Working with deformers](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/modeling-basics/gizmo-3d/deformers/deformers.html)
- [Deformer list and settings](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/modeling-basics/gizmo-3d/deformers/list-of-deformers/list-of-deformers.html)
- [Parametric meshes](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/modeling-basics/gizmo-3d/parametric-meshes/parametric-meshes.html)

This is an independent implementation with the limits above, not identical ZBrush internals.
