#version 330 core
out vec4 FragColor;

in vec2 in_coords;

uniform sampler2D u_tex_depth;
uniform sampler2D u_tex_color;
uniform sampler2D u_tex_debug;

float LinearizeDepth(float depth, float near, float far) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}

void main()
{             
    float near = 0.1;
    float far = 100;
    float scale = 10;

    #if 1
        float depth = texture(u_tex_depth, in_coords).r;
        float count_o = texture(u_tex_debug, in_coords).r;
        float count_t = texture(u_tex_debug, in_coords).g;

        float lin_depth = min(LinearizeDepth(depth, near, far) / scale, 1);
        //vec4 counts_contrib = vec4(0, float(min(count_o, 1))/2, float(min(count_t, 1))/2, 0);
        vec4 counts_contrib = vec4(0, float(min(count_o, 2)), 0, 0)*0.5;
        vec4 depth_contrib = vec4(lin_depth, lin_depth, lin_depth, 0);
        FragColor = counts_contrib*0.75 + depth_contrib*0.25;
    #else
        float depth = texture(u_tex_depth, in_coords).r;
        float min_depth = texture(u_tex_debug, in_coords/16).x;
        float max_depth = texture(u_tex_debug, in_coords/16).y;
        float lin_depth = min(LinearizeDepth(depth, near, far) / scale, 1);
        float lin_min_depth = min(LinearizeDepth(min_depth, near, far) / scale, 1);
        float lin_max_depth = min(LinearizeDepth(max_depth, near, far) / scale, 1);
    
        //FragColor = vec4(lin_depth, lin_depth, lin_depth, 1) * 0.5;
        FragColor = vec4(min_depth, max_depth, lin_depth, 1) * 0.5;
        FragColor = vec4( 
            min_depth > depth?1:0,
            max_depth < depth?1:0,
            lin_depth, 0
        ) * 0.5;
    #endif
}
