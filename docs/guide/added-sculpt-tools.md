# Added sculpt tools

The default interface keeps the existing shelves and brush strip. The new **Sculpt tools** button opens a compact window with the added controls. They are also in the corresponding palettes, so you can customize your own shelves as before.

## Where to find the controls

| Control | Palette | What it does |
| --- | --- | --- |
| ReplayLastRel | Stroke | Repeats the last stroke beginning at the cursor. Use Shift+1 on the model. |
| AdjustLast | Stroke | Scales the current history stroke from -1 to 2. Zero removes its displacement, negative values reverse it. |
| Interpolate, Strokes Count | Stroke | Inserts evenly spaced paths between the two last strokes with the current brush settings. |
| LazyMouse, LazyRadius | Stroke | Stabilizes the path by letting the brush trail the cursor. |
| StoreMT, Switch MT, DelMT | Tool > Morph Target | Stores one surface snapshot per SubTool, swaps it with the current surface, or deletes it. |
| WeldPoints, Close Holes | Tool > Geometry | Welds coincident vertices or caps open boundary loops. |
| GroupsLoops, Panel Loops | Tool > Geometry | Adds inset group-border loops or makes thick, separate group panels. |
| Split Masked Points | Tool > SubTool | Moves masked polygons into a new SubTool. Undo restores both parts. |
| Copy Tool, Paste Tool | Tool > SubTool | Copies and appends a SubTool through the app's mesh clipboard. |
| TPoseMesh, TPose SubT | Transform | Creates a combined pose mesh, then transfers its vertex offsets back to the original visible SubTools. |
| Persp, Floor | Draw > Camera and the right shelf | Switches perspective/orthographic view and toggles a depth-tested world floor grid. |
| Crop factor | Draw > Camera | Changes the camera sensor crop while preserving the displayed focal length. |
| Flip, Double | Tool > Display | Reverses normals or switches between single-sided and double-sided display. |
| Transp, Ghost | Tool > Display and the right shelf | Toggles model opacity or dims inactive SubTools. |

The original view cube, Spotlight reference tools, independent opacity controls, quad DynaMesh, smoothing controls and history remain available.

## Morph Targets

StoreMT captures the current vertex positions and binds them to the mesh's vertex order and triangles. Switch MT swaps the stored and current shapes. The snapshot is saved in a project and carried by that SubTool. Storing, switching and deleting are undoable. Topology changes make a stored target incompatible. Store a new target after remeshing. This release supplies the snapshot controls, not a Morph brush.

## Adjust Last and interpolation

Adjust Last uses Sculpt Timeline's retained displacement for a sculpt stroke on the base mesh. Strokes on layers, topology operations, paint and mask steps cannot be adjusted through this control. Later strokes retain their existing offsets rather than being simulated again.

Interpolate resamples the two recorded screen paths and inserts the selected number of paths between them. Keep the camera unchanged between the two source strokes. It uses the current brush settings and interpolates recorded pressure. Each generated path is a separate undo step. It does not interpolate brush colour, brush size or the settings of the two source strokes.

## Geometry tools

Close Holes fills manifold boundary loops. It rejects branching or degenerate boundaries. Caps can contain triangles. WeldPoints welds coincident positions. Both require Micro to be baked and subdivision levels removed.

GroupsLoops uses a centroid inset for each visible PolyGroup and adds up to eight border rings. Check concave shapes and unusually narrow regions for overlap. Panel Loops builds thick shells from those groups with quad side walls. These are basic independent implementations, with inset, loop-count and thickness controls. They do not reproduce ZBrush's advanced bevel curves or polish modes. Original surface UVs are retained where possible, while new border UVs need Auto UV before texture painting. Sculpt layers on rebuilt topology are baked into the result. Undo restores the original mesh and layers.

## Combined-mesh posing

TPoseMesh combines the visible SubTools into a temporary mesh. Pose it with the existing Gizmo or Transpose line while preserving its topology. TPose SubT transfers the offsets to the original SubTools and removes the temporary mesh. Each original receives its own undo step. **Cancel Pose** discards the temporary result and restores visibility. Finish or cancel this session before saving a project. Changing topology on the pose mesh or an original stops transfer, so undo those topology edits or cancel the pose.

## Optional shelf preset

Preferences > Interface offers **Screenshot layout** and **Classic shelves**. The screenshot preset collects stroke controls at the top, geometry controls at the bottom, brush/colour controls on the left and navigation on the right. It uses the same actions and settings as the palettes. Classic shelves remain the default. Store Config saves your choice. Smaller screens can scroll the optional shelves.

## Documentation used

These independent controls were checked against Maxon's documentation for [Stroke](https://help.maxon.net/zbr/en-us/Content/html/reference-guide/stroke/stroke.html), [Adjust Last](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/sculpting/sculpting-brushes/strokes/adjust-last/adjust-last.html), [Morph Targets](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/sculpting/morph-targets/morph-targets.html), [Geometry](https://help.maxon.net/zbr/en-us/Content/html/reference-guide/tool/polymesh/geometry/geometry.html), [Panel Loops](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/hard-surface/panel-loops/panel-loops.html), and [SubTools](https://help.maxon.net/zbr/en-us/Content/html/reference-guide/tool/polymesh/subtool/subtool.html). The reference screenshot informed placement. This is not a claim of complete ZBrush feature or numerical parity.


## Repeat and mask fixes in 0.10.4

Press **1** to repeat the last recorded brush stroke with its original effective
brush and settings. For example, ClayBuildup with ZSub or Alt held repeats as a
subtracting ClayBuildup stroke, even after selecting Inflate. Shift smoothing,
MaskPen and Ctrl+Alt erasing are recorded too. Replay keeps the current brush
selection and settings ready for your next normal stroke. It remembers the
stroke's alpha, recipe, VDM, size, intensity, focal shift, symmetry, pressure
and paint settings. The camera and target are the current view/active SubTool.

Rectangle, lasso, topology mask, mask blur/sharpen/clear/invert and Knife/Clip
operations also become the last repeatable action. These repeat their recorded
operation and parameters. File operations and arbitrary menu commands are not
repeat targets. Opening or creating a document clears the recording. Recordings
are session-only. Shift+1 is relative replay for brush paths; Interpolate keeps
using the current brush settings.

MaskPen, rectangle/lasso selections and topology masks follow the enabled X,
Y and Z symmetry axes. Shape masks mirror in object space, including in rotated
views, and overlapping shapes apply mask strength once. An existing asymmetric
mask is preserved outside the operation. Hidden vertices remain protected.
