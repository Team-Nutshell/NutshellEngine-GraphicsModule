# NutshellEngine-GraphicsModule - Vulkan Raymarching
![Vulkan Raymarching](https://i.imgur.com/7JPWwwL.png)

NutshellEngine Graphics Module using raymarching to draw.

Place ``raymarching.frag``, ``raymarching_helper.glsl`` and ``scene.glsl``, found in the ``shaders`` directory, near NutshellEngine's executable to use and edit them.

``raymarching.frag``, ``raymarching_helper.glsl`` and ``scene.glsl`` can be edited in real-time and saving one of them will automatically recompile the shader and the pipeline.

- ``raymarching.frag`` is the fragment shader and contains the ``main`` function, the ``raymarch`` function and the lighting functions.
- ``raymarching_helper.glsl`` is included by ``raymarching.frag`` and contains some random and noise functions, basic shape functions, combination operators and values that can be used in the raymarching.
- ``scene.glsl`` is included by ``raymarching.frag`` and contains the scene, with objects, materials, camera and lights, used to draw.

Compilation errors are written in the console.