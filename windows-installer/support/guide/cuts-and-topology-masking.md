# Cuts and topology masking

Hold Ctrl+Shift and choose **KnifeLine** or **ClipLine** in the Brush picker, or choose one under Tool > Visibility. Drag a straight line. The red preview shows the side affected. Hold Alt at release to reverse the side. The enabled X, Y and Z symmetry axes also mirror the planes. Each completed gesture is one undo step.

**KnifeLine** removes geometry on that side and caps closed cut contours, including holes. Unchanged quad faces remain quads; intersections and caps can contain triangles. Use DynaMesh afterward for quad remeshing. The mesh must have closed manifold cut contours. Show all faces, clear masks, bake Micro and remove subdivision levels before cutting. The current surface and vertex color are retained; sculpt and paint layer stacks are baked into the cut result. Undo restores the original mesh and stacks. A cut that would remove the whole SubTool is rejected without changing it.

**ClipLine** pushes vertices onto the plane, preserving faces, quad pairs, UVs and mask. Masked and hidden vertices are protected. It can compress or overlap faces, as expected for a clip operation. Bake Micro and remove subdivision levels first. Clip edits the active sculpt layer if one is selected.

These are straight line tools. Multi-point curves, Knife circle/rectangle, brush modifiers and Transpose extrusion are not implemented in this release.

## Ctrl masking

In Draw mode, Ctrl+drag beginning on the surface paints MaskPen. Ctrl+Alt erases. A Ctrl click blurs the existing mask; Ctrl+Alt click sharpens it.

With MaskPen selected, Ctrl+drag **starting on the canvas background** draws a rectangle. Vertices inside are masked, including the far side. Ctrl+Alt removes mask inside. A rectangle enclosing no visible vertices clears an existing mask, while a background click inverts it. UI panels do not start a canvas selection. MaskLasso and MaskRect remain available through the Ctrl Brush picker.

## Gizmo and Transpose

In Move, Scale or Rotate mode (W/E/R), Ctrl+drag on the model from a joint toward the moving tip creates a soft topology mask: distance is measured along connected surface edges. The body behind the starting point is protected and the tip is free. End the line on the surface. Ctrl+Alt erases the computed mask; Shift added during the drag adds to the mask. Ctrl+click on a PolyGroup masks everything except that group.

Enable **Transpose line** in Gizmo 3D for a three-circle action line. Drag on the model away from a handle to place it. Its start is the pivot, the middle moves/scales/rotates in the view, and the tip moves/scales along the line or rotates in the view. W/E/R selects the operation. The same topology masking gesture works in both modes. Transforms use the existing mask and falloff controls. Undo restores each transform.

## Symmetry feedback and mirrors

The primary contact point and red mirrored contact dots show where the stroke meets the visible surface. X, Y and Z combinations are supported. Overlapping points on an axis plane are merged; points behind other geometry are omitted. The dots remain when the surface-ring preference is off. While an asynchronous solver batch is modifying the mesh, contact picking pauses to avoid reading mutable geometry.

Tool > Geometry > Modify Topology contains Mirror And Weld. Tool > Deformation contains Mirror. Their X/Y/Z toggles can be combined. Mirror reflects the complete mesh; Mirror And Weld retains the selected side and joins its reflection at the axis plane.

## Reference behavior

The interaction design follows these Maxon documentation topics. These independent implementations do not claim numerical or proprietary solver parity with ZBrush.

- https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/sculpting/sculpting-brushes/clip-brushes/clip-brushes.html
- https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/modeling-basics/gizmo-3d/gizmo-3d.html
- https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/masking/masking.html
