#version 330 core

#ifdef FRAG
    out vec4 frag_color;
    
    in vec2 uv;

    uniform sampler2D screen;
    uniform float gamma;
    uniform float exposure;

    void main()
    { 
        float exposure_ = exposure;

        vec3 color = texture(screen, uv).xyz;
        vec3 mapped = vec3(1.0) - exp(-color * exposure_);
        mapped = pow(mapped, vec3(1.0 / gamma));
        //mapped *= 0.5;
        frag_color = vec4(mapped, 1.0);
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec2 a_uv;

    out vec2 uv;

    void main()
    {
        gl_Position = vec4(a_pos.x, a_pos.y, 0.0, 1.0); 
        uv = a_uv;
    }  
#endif