# View cube and Spotlight references

The cube in the upper-right of the canvas shows the current orientation. Click a face or the Front, Back, Left, Right, Top or Bottom button to snap to that direction. The camera target and distance stay in place. Front looks toward the model from +Z. These are perspective axis views.

Click **Reference...** below the cube, or use **Render > Spotlight reference...**. Use **Add / Replace image...**, **File > Add reference image...**, or drop an image on the canvas. PNG, JPEG and the existing image formats are supported. This reference tool is separate from Generate > Image to 3D.

References start locked so you can sculpt and orbit normally. Turn on **Edit placement** to move the image by dragging the canvas. The mouse wheel changes its scale. Shift+drag rotates it. Turn Edit placement off when you are ready to sculpt.

**Image opacity** fades the reference. **Mesh opacity** fades the visible mesh surfaces independently. For silhouette matching, put the reference behind the mesh and reduce Mesh opacity. **In front of mesh** instead overlays the image over the model. Scale, rotation, position, horizontal flip and Reset image placement are also available. **Shift+Z** shows or hides the reference. The Render palette also has Mesh opacity and View Cube controls.

Save the scene as a SculptForge project to keep the reference image, its placement, mesh opacity and camera view. The project embeds a PNG copy, downscaled to a maximum side of 4096 pixels, so the reference does not depend on the original file staying in the same place.

This version supports one active reference image. Add / Replace changes that image. The reference overlay does not project image colours into a texture or paint them onto the mesh.
