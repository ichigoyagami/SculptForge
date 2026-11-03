# Sculpt controls in 0.9.7

Focal Shift is visible on the third toolbar row. Negative values broaden and harden the tip. Positive values narrow and soften it. The O key opens its existing cursor popup. Camera focal length is a separate control in Render; it changes perspective, not brush falloff.

Z Intensity multiplies the brush displacement by intensity/100 and by pen pressure when Pressure > Strength is enabled. Zero intensity or pressure makes no geometric change. Each built-in brush remembers its own intensity, Focal Shift and ZAdd/ZSub state when switching brushes. These settings are saved in projects. Smooth keeps separate intensity and falloff, and uses a round tip rather than inheriting a Clay alpha.

Clay samples the surface before the current stroke and fills toward a bounded plane. ClayBuildup samples the edited surface and keeps adding material when you scrub over the same area. Clay depth in Draw adjusts their target-plane height. Clay follows stroke turns the tip to follow the current surface tangent. This is most visible on ClayBuildup or a shaped alpha; a circular alpha has no visible rotation. Turn the option off for a fixed screen-relative tip.

DamStandard makes a narrow groove across the stroke, with rounded ends and lateral pinching. Its ZSub default cuts; holding Alt makes a ridge. DamStandard pinch in Draw adjusts the side pull from 0 to 100, independently of Z Intensity. A small brush still needs enough mesh vertices to resolve a sharp crease.

Smooth has Standard, Stronger and Volume modes on the toolbar and in Draw. Standard uses positive diffusion, so broad bumps visibly soften. Stronger uses four times as many passes for dense meshes and raised strokes. Volume uses alternating fairing to reduce shrinkage; it deliberately retains more broad form. Start smoothing with Shift, then release Shift while still dragging to use Volume for the remainder of that stroke. Lifting the pen ends that temporary alternative. Hold Shift and press U or O to change only Smooth's intensity or Focal Shift.

Zero passes means Auto: brush radius and squared local edge density choose 4 to 128 passes, multiplied by four for Stronger. A positive value selects 1 to 64 passes before that multiplier. Masked and hidden vertices remain protected. Polygon-edge smoothing ignores quad render diagonals. Higher pass counts cost more time. Smoothing at a lower subdivision level remains useful for changing broad forms while preserving higher-level detail.

Press 1, or use Stroke > Replay last stroke, to retrace the last sculpt/paint cursor path with the current brush settings and the pressure recorded along that path. It creates one undo step. The path is relative to the viewport; changing the view changes where it lands. It can replay on the active SubTool if the start hits its surface. Mask strokes are not recorded. In the SculptForge input style, 1 now repeats instead of selecting Standard; select Standard from its shelf tile or B S T.

The history bar at the top of the viewport belongs to the active SubTool. Drag its slider to undo or redo several steps, or use its Undo/Redo buttons. Switching SubTools switches timelines. Editing an earlier state discards the undone future. The existing memory budget can trim older steps. History is kept for the current session and is not serialized in .sfproj files.

## Reference behavior and limits

The implementation follows publicly documented controls and behavior; it is not an exact numerical reproduction of ZBrush's private implementation. No side-by-side ZBrush runtime comparison was available. Reference pages reviewed:

- [Maxon: Smooth, intensity and Shift-release alternate](https://help.maxon.net/zbr/en-us/Content/html/user-guide/3d-modeling/sculpting/sculpting-brushes/smooth/smooth.html)
- [Maxon: Smooth Brush Modifiers](https://help.maxon.net/zbr/en-us/Content/html/reference-guide/brush/smooth-brush-modifiers/smooth-brush-modifiers.html)
- [Maxon: Samples and Buildup](https://help.maxon.net/zbr/en-us/Content/html/reference-guide/brush/samples/samples.html)
- [Maxon: per-brush Z Intensity](https://help.maxon.net/zbr/en-us/Content/html/getting-started/your-first-creature/your-first-creature.html)
- [Maxon: ReplayLast](https://help.maxon.net/zbr/en-us/Content/html/reference-guide/stroke/modifiers/modifiers.html)
- [ZBrushCentral: weak Smooth on dense meshes](https://www.zbrushcentral.com/t/smoothing-bruch-has-no-effect-anymore/352836)
- [ZBrushCentral: DamStandard pinch control](https://www.zbrushcentral.com/t/how-do-i-decrease-the-pinch-on-the-dam-standard/460137)

New UI spheres have quad topology. Imported triangle meshes keep their topology until you remesh them. The CLI new_sphere command preserves its earlier triangle sphere default; quads=true requests the new quad sphere.

DynaMesh rebuilds the active SubTool as a voxel surface with real quad faces. The button and resolution are on the toolbar and under Tool > DynaMesh. Higher resolution creates more quads and preserves smaller shapes, at greater memory cost. It merges overlapping components within the active SubTool, bakes sculpt layers and replaces topology. Undo restores the previous mesh. UVs need rebuilding; Keep Texture projects a painted texture onto new UVs. A rebuild runs in the background, and changes to the source cause its result to be discarded. Dynamic topology is turned off after a successful DynaMesh so the next stroke keeps the new quads. Enabling Dynamic later can introduce triangles again.

Quad faces still draw as triangles internally on the GPU. Their polygon boundaries, wireframe and supported exports remain quads. DynaMesh shows both its quad count and any unpaired triangles. QRemesh remains available when you want a target polygon count and more directed edge flow.
