# Custom merged build 0.10.8

Merged the supplied upstream 0.10.7 tree with custom 0.10.5 changes, using upstream 0.10.1 as the common base. This includes the new upstream PBR/fill-layer systems, UV editing, packs/macros, selection sets, radial/local symmetry, deformations, tablet/navigation fixes, optional themes and layouts.

Earlier custom tools are retained: orientation cube, Spotlight images and opacity, mesh opacity, quad sphere and quad DynaMesh, polygon-aware smoothing/normals, calibrated Clay/ClayBuildup/DamStandard, Focal Shift and Z Intensity, symmetry contact dots and masking, recorded key-1 replay, per-SubTool history and branching timeline, Knife/Clip/Slice, Delete Hidden, backface masking and the studio tools. Upstream labels ClayBuildup as Clay Block and DamStandard as Sharp Crease. Classic extended shelves and ZBrush-style input are the startup defaults; saved preferences remain respected.

The new Gizmo gear opens Transform Type. Eighteen interactive modifiers support preview, reset, cancel and one-step accept/undo. Existing geometry commands and primitive append buttons are included. Pivot operators support sticky placement, center, mesh to axis, orientation reset, lock/unlock and all-visible transforms. See GIZMO-DEFORMERS.md for exact behavior and unavailable operators.

The implementation is independently written. It does not claim identical ZBrush internals, full operator coverage or identical tablet feel. Keep an original project backup when testing topology tools on production meshes.
