#version 330 core

#ifdef FRAG
    out vec4 frag_color;

    uniform vec3 color;

    void main()
    {
        float originalZ = gl_FragCoord.z / gl_FragCoord.w;
        vec3 mixed_color = color * 1/originalZ;
        frag_color = vec4(mixed_color, 1.0);
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    uniform vec3 color;

    void main()
    {
        gl_Position = projection * view * model * vec4(a_pos, 1.0);
    } 
#endif