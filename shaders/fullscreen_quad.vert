#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 in_coords;

void main()
{
    in_coords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}

