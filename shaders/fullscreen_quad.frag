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
    float count_o = texture(u_tex_debug, in_coords).r;
    float count_t = texture(u_tex_debug, in_coords).g;

    float lin_depth = min(LinearizeDepth(depth, near, far) / scale, 1);

    vec4 counts_contrib = count_o*vec4(0, 1, 1, 0);
    vec4 depth_contrib = vec4(1, 1, 1, 0);
    FragColor = counts_contrib*0.5 + depth_contrib*0.4;
    gl_FragDepth = depth;
}
