#version 330 core

#ifdef FRAG
    out vec4 o_color;
    
    in VS_OUT
    {
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
    } _in;

    uniform sampler2D u_map_diffuse;
    void main()
    {
        o_color = texture(u_map_diffuse, _in.uv);
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec2 a_uv;
    layout (location = 5) in mat4 a_model;

    uniform mat4 u_view;
    uniform mat4 u_projection;
    uniform sampler2D u_map_diffuse;
    
    out VS_OUT
    {
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
    } _out;

    void main()
    {
        vec4 world_pos = u_projection * u_view * a_model * vec4(a_pos, 1.0);
        gl_Position = world_pos;
        _out.frag_pos = world_pos.xyz;
        _out.uv = a_uv;
        _out.norm = vec3(1, 0, 0);
    } 
#endif