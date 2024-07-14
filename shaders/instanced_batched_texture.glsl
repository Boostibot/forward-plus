#version 460 core
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_shader_storage_buffer_object : require

#define MAX_BATCH 8
#define MAX_LIGHTS 32
#define MAX_RESOULTIONS 32

struct Light{
    vec4 pos_and_range;
    vec4 color_and_radius;
};

layout(std140) uniform Environment {
    mat4 projection;
    mat4 view;
    vec4 view_pos;
    vec4 base_illumination;

    float light_linear_attentuation;
    float light_quadratic_attentuation;
    float gamma;
    int   lights_count;

    Light lights[MAX_LIGHTS];
} env;

struct Params_Data {
    vec4 diffuse_color;
    vec4 ambient_color;
    vec4 specular_color;
    float specular_exponent;    
    float metallic;             

    int map_diffuse; 
    int map_specular; 
    int map_normal; 
    int map_ambient; 

    //mat3x2 map_diffuse_transform;
    //mat3x2 map_specular_transform;
    //mat3x2 map_normal_transform;
};

layout(std430, binding = 0) buffer Params {
    Params_Data params[];  
};

uniform sampler2DArray u_map_resolutions[MAX_RESOULTIONS];

struct Map {
    int resolution;
    int layer;
};

Map map_decode(int map)
{
    Map res;
    res.resolution = map >> 16;
    res.layer = map & 0xFFFF;
    return res;
}

#ifdef FRAG
    out vec4 o_color;
    
    in VS_OUT { 
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
        flat int batch_index;
    } _in;

    vec4 map_sample(Map map)
    {
        return texture(u_map_resolutions[map.resolution - 1], vec3(_in.uv.xy, map.layer));
    }

    vec4 map_sample_or(int map, vec4 if_not_found)
    {
        Map map_ = map_decode(map);
        if(map_.resolution > 0)
            return map_sample(map_);
        else
            return if_not_found;
    }

    vec3 vec3_of(float val)
    {
        return vec3(val, val, val);    
    }

    vec3 compute()
    {
        int bi = _in.batch_index;
        //Most common value for specular intensity
        const float base_specular_exponent = 10;

        Params_Data param = params[bi];

        vec3  diffuse_color = map_sample_or(param.map_diffuse, param.diffuse_color).xyz;
        vec3  specular_color = map_sample_or(param.map_specular, param.specular_color).xyz;
        vec3  ambient_color = map_sample_or(param.map_ambient, param.ambient_color).xyz;

        float metallic = sqrt(param.metallic); //to better follow the intuitive linearity of this parameter
        float specular_exponent = param.specular_exponent;

        //so that when extra expontiated extra shiny!
        float specular_intensity = log(specular_exponent / base_specular_exponent + 1);
        //specular_intensity = 1;

        vec3 diffuse_illumination = env.base_illumination.xyz;
        vec3 specular_illumination = vec3(0, 0, 0);
        
        vec3 normal = normalize(_in.norm);
        vec3 view_dir = normalize(env.view_pos.xyz - _in.frag_pos);

        for(int i = 0; i < env.lights_count; i++)
        {
            float light_range = env.lights[i].pos_and_range.w;
            vec3 light_pos = env.lights[i].pos_and_range.xyz;
            vec3 light_color = env.lights[i].color_and_radius.xyz;

            float light_dist = length(light_pos - _in.frag_pos);
            float attentuation = 1;
            attentuation = 1 / (1 + light_dist*light_dist*env.light_quadratic_attentuation + light_dist*env.light_linear_attentuation);

            //If too far skip or too irrelavant skip
            if(light_dist <= light_range && attentuation > 0.0001)
            {
                vec3 light_dir = normalize(light_pos - _in.frag_pos);
                
                vec3 reflect_dir = reflect(-light_dir, normal);
                vec3 halfway_dir = normalize(light_dir + view_dir);  

                float diffuse_mult = max(dot(light_dir, normal), 0.0);
                float specular_mult_uncapped = pow(max(dot(normal, halfway_dir), 0) + 0.0001, specular_exponent);

                // correct the specular so that there is no specular where diffuse is zero.
                // we do custom falloff similar to the curve of x^(1/4) because it looks bad when we simply
                // cut it off when diffuse_mult is zero
                float epsilon = 0.05;
                float sharpness = 2;
                float specular_correction = 1 - pow(abs(min(diffuse_mult - epsilon, 0))/epsilon, sharpness);
                float specular_mult = specular_mult_uncapped * specular_correction;

                diffuse_illumination += light_color * diffuse_mult * attentuation;
                specular_illumination += light_color * specular_mult * attentuation;
            }
        }
        
        vec3 diffuse_sum = diffuse_color * diffuse_illumination * (1 - metallic);

        vec3 reflection_color = mix(specular_color, diffuse_color, metallic);
        vec3 specular_sum = reflection_color * specular_illumination * specular_intensity;
        vec3 ambient_sum = ambient_color;

        vec3 result = ambient_sum + diffuse_sum + specular_sum;
        return result;
    }

    void main()
    {
        o_color = vec4(compute().xyz, 1);
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec2 a_uv;
    layout (location = 2) in vec3 a_norm;
    layout (location = 3) in vec3 a_tan;
    layout (location = 4) in mat4 a_model;
    
    out VS_OUT { 
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
        flat int batch_index;
    } _out;

    mat4 BuildTranslation(vec3 delta)
    {
        return mat4(
            vec4(1.0, 0.0, 0.0, 0.0),
            vec4(0.0, 1.0, 0.0, 0.0),
            vec4(0.0, 0.0, 1.0, 0.0),
            vec4(delta, 1.0));
    }
    void main()
    {
        vec4 fragment_pos = a_model * vec4(a_pos, 1.0);
        //mat4 model = BuildTranslation(vec3(gl_InstanceID, 0, 0));

        vec4 world_pos = env.projection * env.view * a_model * vec4(a_pos, 1.0);
        
        _out.frag_pos = fragment_pos.xyz;
        _out.uv = a_uv;
        _out.norm = a_norm;
        _out.batch_index = gl_DrawID;

        gl_Position = world_pos;
    } 
#endif