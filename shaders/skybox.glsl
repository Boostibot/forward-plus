#version 330 core

#ifdef FRAG
    out vec4 frag_color;

    in vec3 uv;

    uniform samplerCube cubemap_diffuse;
    uniform float gamma;

    vec3 gamma_correct(vec3 color, float gamma)
    {
        return pow(color, vec3(1.0 / gamma));
    }

    void main()
    {    
        vec4 texture_color = texture(cubemap_diffuse, uv);
        vec3 corrected_texture_color = gamma_correct(vec3(texture_color), 1.0/gamma);

        frag_color = vec4(corrected_texture_color, 1.0);
        //frag_color = vec4(uv, 1.0);
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;
    
    out vec3 uv;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform float gamma;

    void main()
    {
        vec4 world_pos = model * vec4(a_pos, 1.0);
        //flip z because all cubemaps use left handed systems in opengl
        uv = vec3(world_pos.xy, -world_pos.z);

        vec4 pos = projection * view * world_pos;
        gl_Position = pos.xyww;
    }
#endif