# NutshellEngine-GraphicsModule - Vulkan Shader Editor
![Vulkan Shader Editor](https://i.imgur.com/GW6aKyf.png)

NutshellEngine Graphics Module using a fragment shader to draw.

Place ``shader.frag`` found in the ``shaders`` directory, near NutshellEngine's executable to use and edit it.

``shader.frag`` can be edited in real-time and saving it will automatically recompile the shader and the pipeline.

Pre-defined variables:
- **uv** (*vec2*): texture coordinates, (0.0, 0.0) being the top-left corner.
- **time** (*float*): current time, in seconds.
- **pC.width** (*uint*) and **pC.height** (*uint*): window size.
- **pC.cameraPosition** (*vec3*) and **pC.cameraDirection** (*vec3*): camera position and direction, if the ECS contains one.
- **outColor** (*vec4*): output color.