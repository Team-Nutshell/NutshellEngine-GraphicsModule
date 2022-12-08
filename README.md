# NutshellEngine-GraphicsModule - Vulkan Raymarching
![Vulkan Raymarching](https://i.imgur.com/Nl8WA33.png)

NutshellEngine Graphics Module using raymarching to draw.

Place ``raymarching.frag``, ``raymarching_helper.glsl`` and ``pbr_helper.glsl``, found in the ``shaders`` directory, near NutshellEngine's executable to use and edit them.

``raymarching.frag``, ``raymarching_helper.glsl`` and ``pbr_helper.glsl`` can be edited in real-time and saving ``raymarching.frag`` will automatically recompile the shader and the pipeline.

- ``raymarching.frag`` is the fragment shader and contains the ``main`` function, the description of the scene and the ``raymarch`` function.
- ``raymarching_helper.glsl`` is included by ``raymarching.frag`` and contains some basic shape functions, operators and values that can be used in the raymarching.
- ``pbr_helper.glsl`` is included by ``raymarching.frag`` and contains the BRDF that can be used for shading.

Compilation errors are written in the console.