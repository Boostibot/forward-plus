#version 330 core

#ifdef FRAG
    out vec4 frag_color;

    uniform vec3 color;
    uniform float gamma;

    vec3 gamma_correct(vec3 color, float gamma)
    {
        return pow(color, vec3(1.0 / gamma));
    }

    void main()
    {
        frag_color = vec4(gamma_correct(color, 1.0/gamma), 1.0);
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