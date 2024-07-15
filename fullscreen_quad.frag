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
    float depth = texture(u_tex_depth, in_coords).r;
    //float depth_max = texture(u_tex_debug, in_coords).r;
    //float depth_min = texture(u_tex_debug, in_coords).g;
    float count_o = texture(u_tex_debug, in_coords).r;
    float count_t = texture(u_tex_debug, in_coords).g;

    float lin_depth = min(LinearizeDepth(depth, near, far) / scale, 1);

    vec4 counts_contrib = vec4(0, float(count_o)/2, float(count_t)/2, 0);
    vec4 depth_contrib = vec4(lin_depth, lin_depth, lin_depth, 0);

    //FragColor = vec4(vec3(lin_depth), 1.0);
    //if(count_t > 0)
    //    FragColor = vec4(1, 1, 1, 1);
    //else
    FragColor = counts_contrib*0.65 + depth_contrib*0.35;

        //FragColor = vec4(float(count_o)/2, depth, float(count_t)/2, 1);
    //FragColor = vec4(in_coords, 0, 1);
}
