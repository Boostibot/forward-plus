#version 330 core

#ifdef FRAG
    out vec4 frag_color;
    
    in GS_OUT
    {
        vec3 color;
    } fs_in;

    void main()
    {
        frag_color = vec4(fs_in.color, 1.0);
        //frag_color = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec2 a_uv; 
    layout (location = 2) in vec3 a_norm; 
    layout (location = 3) in vec3 a_tan; 
    layout (location = 4) in vec3 a_bitan; 
    
    out VS_OUT
    {
        vec3 norm;
        vec3 tan;
        vec3 bitan;

    } vs_out;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform mat3 normal_matrix;

    void main()
    {
        vs_out.norm = normal_matrix * a_norm;
        vs_out.tan = normal_matrix * a_tan;
        vs_out.bitan = normal_matrix * a_bitan;

        gl_Position = model * vec4(a_pos, 1.0);
    }
#endif

#ifdef GEOM
    layout (triangles) in;
    layout (line_strip, max_vertices = 36) out;

    in VS_OUT
    {
        vec3 norm;
        vec3 tan;
        vec3 bitan;
    } gs_in[];

    out GS_OUT
    {
        vec3 color;
    } gs_out;

    uniform mat4 projection;
    uniform mat4 model;
    uniform mat4 view;

    #define ARROW_SCALE_MULT    0.1
    #define ARROW_OFFSET_SCALED 0.1
    #define ARROW_OFFSET_FLAT   0.001
    #define ARROW_TIP_LEN_MULT  (1.0/8.0)
    #define ARROW_TIP_SPAN_MULT (1.0/16.0)

    void generate_line(vec3 color, vec3 dir, vec4 start_pos_world, float mag)
    {
        gs_out.color = color;
        vec4 start_pos = view * start_pos_world;
        vec4 end_pos = view * (start_pos_world + vec4(dir * mag, 0.0));
        
        gs_out.color = color;
        gl_Position = projection * start_pos;
        EmitVertex();
        
        gl_Position = projection * end_pos;
        EmitVertex();
        EndPrimitive();

        vec4 diff = end_pos - start_pos;
        vec4 perpendicular = diff;
        perpendicular.x = -diff.y;
        perpendicular.y = diff.x;
        
        diff *= ARROW_TIP_LEN_MULT;
        perpendicular *= ARROW_TIP_SPAN_MULT;

        const float FACTOR = 1.0;

        gs_out.color = color;
        gl_Position = projection * end_pos;
        EmitVertex();

        vec4 offset2 = (-diff + perpendicular) * FACTOR;
        vec4 end2 = end_pos + offset2;

        gl_Position = projection * end2;
        EmitVertex();
        EndPrimitive();

        
        gs_out.color = color;
        gl_Position = projection * end_pos;
        EmitVertex();

        vec4 offset3 = (-diff - perpendicular) * FACTOR;
        vec4 end3 = end_pos + offset3;

        gl_Position = projection * end3;
        EmitVertex();
        EndPrimitive();
    }

    #define AVG_MEMBER(arr, prop) (arr[0].prop + arr[1].prop + arr[2].prop) / 3.0

    void generate_offset_line(vec3 color, vec3 dir, vec4 start_pos_world, float mag)
    {
        vec4 norm_offset = vec4(normalize(dir), 0.0) * (ARROW_OFFSET_SCALED * mag + ARROW_OFFSET_FLAT);
        vec4 start_pos = start_pos_world + norm_offset;
        generate_line(color, dir, start_pos, mag);
    }

    void main()
    {
        vec4 edge1 = gl_in[1].gl_Position - gl_in[0].gl_Position;
        vec4 edge2 = gl_in[2].gl_Position - gl_in[0].gl_Position;
        vec3 calculted_norm = normalize(cross(edge1.xyz, edge2.xyz));
        float mag = (length(edge1) + length(edge2)) * ARROW_SCALE_MULT;

        vec3 red   = vec3(1.0, 0.0, 0.0);
        vec3 green = vec3(0.0, 1.0, 0.0);
        vec3 blue  = vec3(0.0, 0.0, 1.0);
        
        generate_offset_line(green, AVG_MEMBER(gs_in, norm), AVG_MEMBER(gl_in, gl_Position), mag);
        generate_offset_line(red, calculted_norm, AVG_MEMBER(gl_in, gl_Position) + vec4(0.01), mag);

        if(false)
        {
            generate_offset_line(red, gs_in[0].norm, gl_in[0].gl_Position, mag);
            generate_offset_line(green, gs_in[1].norm, gl_in[1].gl_Position, mag);
            generate_offset_line(blue, gs_in[2].norm, gl_in[2].gl_Position, mag);
        }
        
    }  
#endif