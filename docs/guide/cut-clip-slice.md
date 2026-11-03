# Clip, Knife, Slice and Delete Hidden

Choose tools in **Tool > Visibility** or hold **Ctrl+Shift** and click the
current Brush thumbnail. Ctrl+Shift temporarily activates the selected tool.

| Tool | Result |
| --- | --- |
| ClipLine / ClipCurve | Push existing points onto the shaded side's line or curve. Alt reverses. Topology remains unchanged; masks and hidden points are protected. |
| ClipRect / ClipCircle | Push outside points to the border. Alt pushes inside points outward. These reshape existing openings without cutting new holes. |
| KnifeLine / KnifeCurve | Remove the shaded side and close the cut with a new PolyGroup. Untouched disconnected pieces remain. Caps have paired quads where possible and transition triangles. |
| SliceLine / SliceCurve / SliceRect / SliceCircle | Add topology and PolyGroups while keeping both sides connected. Standard symmetry is deliberately ignored, following ZBrush Slice. |

For curve tools, tap Alt once for a rounded point, twice at the same location
for a sharp corner. Hold Alt at release to reverse the side. Space + drag moves
the live selection. Escape cancels it. Square and Center are available under
Visibility and in the Stroke palette for rectangles and ellipses.

Slice enables PolyGroup colors. Choose SelectRect/SelectLasso and Ctrl+Shift-click
a group to isolate it. SubTool > Split groups separates groups into SubTools;
Close Holes can then close their open borders. Slice itself keeps shared borders.

Delete Hidden already existed at **Tool > Visibility > Delete hidden**. It is
also exposed at **Tool > Geometry > Modify Topology > Delete Hidden** and F3
search. Hide unwanted faces first, then delete. Undo restores them. This removes
faces and unused points; Close Holes is a separate operation.

Key 1 repeats recorded Clip/Knife/Slice parameters. The earlier recorded-brush
replay, masking symmetry, reference images, view cube and custom tools remain.

## Limits

KnifeCurve currently supports open curves with a convex kept region. Unsupported
profiles are rejected before changes. KnifeLasso, KnifeRect and KnifeCircle are
not added. Caps are not uniform quad grids and can retain transition triangles.
New geometry may need Auto UV. Bake Micro/remove subdivision levels first.
Knife requires visible geometry and cleared masks; Clip supports masks.

Slice interpolates masks, colors, UVs and sculpt layers and retains hidden-face
groups. Extra divisions may extend along segment planes outside the visible
group border. SliceCircle uses 48 segments. BRadius and advanced Knife
split-to-parts settings are not implemented. These are independent equivalents,
not exact copies of ZBrush's geometry algorithms.

## References

- [Maxon: Clip](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/hard-surface/clip-brushes/clip/clip.html)
- [Maxon: Knife Brushes](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/hard-surface/clip-brushes/knife-brushes/knife-brushes.html)
- [Maxon: Slice](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/hard-surface/clip-brushes/slice/slice.html)
