#version 330 core

#ifdef FRAG
    out vec4 frag_color;
    
    in VS_OUT
    {
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
    } fs_in;

    uniform vec3 view_pos;
    uniform vec3 light_pos;
    uniform vec3 light_color;
    uniform vec3 ambient_color;
    uniform vec3 specular_color;
    uniform float specular_exponent;

    uniform float light_linear_attentuation;
    uniform float light_quadratic_attentuation;
    uniform float gamma;

    uniform sampler2D map_diffuse;

    float softmax(float a, float b, float hardness, float fade_space)
    {
        float x = max(a, b);
        float u = fade_space;
        float c = -2/(hardness - 1); // rescaled hardness so that at 0 the c = 2 and at 1 c = inf

        return pow(pow(x, c) + pow(u, c), 1/c) - u;
    }

    vec3 gamma_correct(vec3 color, float gamma)
    {
        return pow(color, vec3(1.0 / gamma));
    }

    void main()
    {
        vec3 texture_color = texture(map_diffuse, fs_in.uv).rgb;
        vec3 corrected_texture_color = gamma_correct(texture_color, 1.0/gamma);

        vec3 diffuse = corrected_texture_color * light_color;
        vec3 specular = specular_color * light_color;
        vec3 ambient = ambient_color;

        vec3 light_dir = normalize(light_pos - fs_in.frag_pos);
        vec3 normal = normalize(fs_in.norm);
        vec3 view_dir = normalize(view_pos - fs_in.frag_pos);
        vec3 reflect_dir = reflect(-light_dir, normal);
        vec3 halfway_dir = normalize(light_dir + view_dir);  

        float diffuse_mult = max(dot(light_dir, normal), 0.0);
        float specular_mult_uncapped = pow(max(dot(normal, halfway_dir), 0), specular_exponent);
        
        // correct the specular so that there is no specular where diffuse is zero.
        // we do custom falloff similar to the curve of x^(1/4) because it looks bad when we simply
        // cut it off when diffuse_mult is zero
        float epsilon = 0.05;
        float sharpness = 2;
        float specular_correction = 1 - pow(abs(min(diffuse_mult - epsilon, 0))/epsilon, sharpness);
        float specular_mult = specular_mult_uncapped * specular_correction;

        //attentuation
        float light_dist = length(light_pos - view_pos);
        float atten_lin = light_linear_attentuation;
        float atten_qua = light_quadratic_attentuation;
        float attentuation = 1 / (1 + light_dist*light_dist*atten_qua + light_dist*atten_lin);
        attentuation = 1;

        diffuse = diffuse * diffuse_mult * attentuation;
        specular = specular * specular_mult * attentuation;

        vec3 result = ambient + diffuse + specular;
        frag_color = vec4(result, 1.0);
        return;
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec2 a_uv; 
    layout (location = 2) in vec3 a_norm; 
    
    out VS_OUT
    {
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
    } vs_out;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform mat3 normal_matrix;

    void main()
    {
        vs_out.frag_pos = vec3(model * vec4(a_pos, 1.0));
        vs_out.uv = a_uv;
        vs_out.norm = normal_matrix * a_norm;

        gl_Position = projection * view * model * vec4(a_pos, 1.0);
    }
#endif