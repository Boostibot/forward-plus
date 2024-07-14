#version 330 core

#ifdef FRAG
    out vec4 frag_color;
    
    in VS_OUT
    {
        vec3 frag_pos;
        vec2 uv;
        vec3 norm;
    } fs_in;

    #define PBR_MAX_LIGHTS 4

    uniform samplerCube cubemap_irradiance;
    uniform samplerCube cubemap_prefilter;
    uniform sampler2D texture_brdf_lut;

    uniform sampler2D texture_albedo;
    uniform sampler2D texture_normal;
    uniform sampler2D texture_metallic;
    uniform sampler2D texture_roughness;
    uniform sampler2D texture_ao;

    uniform int use_textures;

    uniform vec3 solid_albedo;
    uniform float solid_metallic;
    uniform float solid_roughness;
    uniform float solid_ao;
    uniform vec3  reflection_at_zero_incidence;

    uniform float gamma;
    uniform float attentuation_strength;

    uniform vec3  view_pos;
    uniform vec3  lights_pos[PBR_MAX_LIGHTS];
    uniform vec3  lights_color[PBR_MAX_LIGHTS];
    uniform float lights_radius[PBR_MAX_LIGHTS];

    const float PI = 3.14159265359;

    vec3 gamma_correct(vec3 color, float gamma)
    {
        return pow(color, vec3(1.0 / gamma));
    }

    vec3 fresnel_schlick(float cos_theta, vec3 F0)
    {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    }  

    float distribution_ggx(vec3 N, vec3 H, float roughness)
    {
        float a      = roughness*roughness;
        float a2     = a*a;
        float NdotH  = max(dot(N, H), 0.0);
        float NdotH2 = NdotH*NdotH;
        
        float num   = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        
        return num / denom;
    }

    float geometry_schlick_ggx(float NdotV, float roughness)
    {
        float r = (roughness + 1.0);
        float k = (r*r) / 8.0;

        float num   = NdotV;
        float denom = NdotV * (1.0 - k) + k;
        
        return num / denom;
    }

    float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
    {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float ggx2  = geometry_schlick_ggx(NdotV, roughness);
        float ggx1  = geometry_schlick_ggx(NdotL, roughness);
        
        return ggx1 * ggx2;
    }

    float attentuate_square(float light_distance, float light_radius)
    {
        return 1.0 / (light_distance * light_distance);
    }

    //from: http://www.cemyuksel.com/research/pointlightattenuation/
    float attentuate_no_singularity(float light_distance, float light_radius)
    {
        float d = light_distance;
        float r = light_radius;
        float d2 = d*d;
        float r2 = r*r;

        float result = 2 / (d2 + r2 + d*sqrt(d2 + r2));
        return result;
    }

    vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness)
    {
        return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    } 

    float fresnel_schlick_roughness2(float cos_theta, float F0, float roughness)
    {
        return F0 + (max(1.0 - roughness, F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    } 

    float clamp01(float f)
    {
        return clamp(f, 0, 1);
    }

    struct Point_Light
    {
        vec3 pos;
        vec3 color;
        float radius;
        float range;
    };

    struct Light_Contribution
    {
        vec3 specular;
        vec3 diffuse;
    };

    Light_Contribution contribution_brdf(vec3 L, vec3 N, vec3 V, vec3 F0, float roughness)
    {
        Light_Contribution result = Light_Contribution(vec3(0), vec3(0));

        vec3 H = normalize(V + L);
        float cos_theta = max(dot(H, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);    
        float NDF = distribution_ggx(N, H, roughness);       
        float G   = geometry_smith(N, V, L, roughness);  
        vec3  F   = fresnel_schlick(cos_theta, F0);

        vec3 specular_numerator    = NDF * G * F;
        float specular_denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
            
        vec3 kS = vec3(F);
        vec3 kD = (vec3(1.0) - kS);

        result.specular = specular_numerator / specular_denominator * NdotL;
        result.diffuse = kD * NdotL;
        return result;
    }

    Light_Contribution contribution_point_light(vec3 P, vec3 N, vec3 V, vec3 R, vec3 F0, float roughness, Point_Light light)
    {
        Light_Contribution result = Light_Contribution(vec3(0), vec3(0));

        vec3 L_whole = light.pos - P;
        float light_distance = length(L_whole);
        if(light_distance < light.range)
        {
            //vec3 point_L = light.pos - P;
            //vec3 center_to_ray = dot(point_L, R)*R - point_L;
            //vec3 closest_point = point_L + center_to_ray * clamp(light.radius/length(center_to_ray), 0, 1);
            //L_whole = point_L;
            vec3 L = normalize(L_whole);

            float attenuation = attentuate_no_singularity(light_distance, light.radius);
            attenuation       = mix(1, attenuation, attentuation_strength);

            Light_Contribution BRDF = contribution_brdf(L, N, V, F0, roughness);
            result.specular = max(BRDF.specular * light.color * attenuation, 0);
            result.diffuse = max(BRDF.diffuse * light.color * attenuation, 0);
        }
        return result;
    }

    //taken from: https://github.com/turanszkij/WickedEngine/blob/62d1d02691286cc6c25da61294bfb416d018782b/WickedEngine/lightingHF.hlsli#L368
    float illuminance_sphere_or_disk(float cosTheta, float sinSigmaSqr)
    {
        float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

        float illuminance = 0.0f;
        // Note: Following test is equivalent to the original formula. 
        // There is 3 phase in the curve: cosTheta > sqrt(sinSigmaSqr), 
        // cosTheta > -sqrt(sinSigmaSqr) and else it is 0 
        // The two outer case can be merge into a cosTheta * cosTheta > sinSigmaSqr 
        // and using saturate(cosTheta) instead. 
        if (cosTheta * cosTheta > sinSigmaSqr)
        {
            illuminance = PI * sinSigmaSqr * clamp(cosTheta, 0, 1);
        }
        else
        {
            float x = sqrt(1.0f / sinSigmaSqr - 1.0f); // For a disk this simplify to x = d / r 
            float y = -x * (cosTheta / sinTheta);
            float sinThetaSqrtY = sinTheta * sqrt(1.0f - y * y);
            illuminance = (cosTheta * acos(y) - x * sinThetaSqrtY) * sinSigmaSqr + atan(sinThetaSqrtY / x);
        }

        return max(illuminance, 0.0f);
    }

    Light_Contribution contribution_sphere_light(vec3 P, vec3 N, vec3 V, vec3 R, vec3 F0, float roughness, Point_Light light)
    {
        Light_Contribution result = Light_Contribution(vec3(0), vec3(0));

        vec3 L_point = light.pos - P;
        vec3 center_to_ray = dot(L_point, R)*R - L_point;
        vec3 closest_point = L_point + center_to_ray * clamp(light.radius/length(center_to_ray), 0, 1);
        vec3 L = normalize(closest_point);
        float light_distance = length(L_point);
        if(light_distance < light.range)
        {
            //float cos_theta = clamp(dot(N, L), -0.999, 0.999); // Clamp to avoid edge case 
            //float sin_sigma = min(light.radius / light_distance, 0.9999f);
            //float f_light = illuminance_sphere_or_disk(cos_theta, sin_sigma * sin_sigma);

            //result.specular = vec3(f_light*f_light);
            //return result;

            float f_light = attentuate_no_singularity(light_distance, light.radius);
            f_light       = mix(1, f_light, attentuation_strength);
            //f_light = 1;
            //f_light *= 1/(light.radius*light.radius*PI);

            Light_Contribution BRDF = contribution_brdf(L, N, V, F0, roughness);
            result.specular = max(BRDF.specular * light.color * f_light, 0);
            result.diffuse = max(BRDF.diffuse * light.color * f_light, 0);
        }
        return result;
    }

    Light_Contribution contribution_ambient_map(vec3 P, vec3 N, vec3 V, vec3 R, vec3 F0, float roughness)
    {
        vec3 F = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);
        
        vec3 kS = F;
        vec3 kD = 1.0 - kS;

        vec3 irradiance = texture(cubemap_irradiance, N).rgb;
        vec3 diffuse    = irradiance;
        
        // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefiltered_color = textureLod(cubemap_prefilter, R,  roughness * MAX_REFLECTION_LOD).rgb;    
        vec2 brdf  = texture(texture_brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefiltered_color * (F * brdf.x + brdf.y);

        Light_Contribution result = Light_Contribution(vec3(0), vec3(0));
        result.diffuse = kD * diffuse;
        result.specular = specular;

        return result;
    }

    vec3 pbr(vec3 albedo, vec3 normal, float metallic, float roughness, float ao)
    {
        vec3 P = fs_in.frag_pos;
        vec3 N = normal; 
        vec3 V = normalize(view_pos - P);
        vec3 R = reflect(-V, N); 

        roughness = max(roughness, 0.005);
        vec3 F0 = mix(reflection_at_zero_incidence, albedo, metallic);

        vec3 direct_light = vec3(0);
        vec3 ambient_light = vec3(0);
        for(int i = 0; i < PBR_MAX_LIGHTS; ++i) 
        {
            Point_Light point_light = Point_Light(vec3(0), vec3(0), 0, 0);
            point_light.pos = lights_pos[i];
            point_light.color = lights_color[i];
            point_light.radius = lights_radius[i];
            point_light.range = 99999;

            Light_Contribution contribution = contribution_sphere_light(P, N, V, R, F0, roughness, point_light);
            contribution.diffuse *= albedo * (1.0 - metallic) / PI;
            direct_light += contribution.diffuse + contribution.specular;
        }
        
        Light_Contribution ambient = Light_Contribution(vec3(0), vec3(0));
        ambient = contribution_ambient_map(P, N, V, R, F0, roughness);
        ambient.diffuse *= albedo * (1.0 - metallic);

        ambient_light = (ambient.diffuse + ambient.specular)*ao;
        
        vec3 color = ambient_light + direct_light;
        return color;
    }

    //old!
    vec3 point_light_pbr(vec3 albedo, vec3 normal, float metallic, float roughness, float ao)
    {
        vec3 N = normal; 
        vec3 V = normalize(view_pos - fs_in.frag_pos);
        vec3 R = reflect(-V, N); 

        vec3 Lo = vec3(0.0);
        vec3 F0 = mix(reflection_at_zero_incidence, albedo, metallic);
        for(int i = 0; i < PBR_MAX_LIGHTS; ++i) 
        {
            vec3 L = normalize(lights_pos[i] - fs_in.frag_pos);
            vec3 H = normalize(V + L);

            float distance    = length(lights_pos[i] - fs_in.frag_pos);
            float radius      = lights_radius[i];
            //float attenuation = attentuate_square(distance, radius);
            float attenuation = attentuate_no_singularity(distance, radius);
            attenuation = mix(1, attenuation, attentuation_strength);
            vec3 radiance     = lights_color[i] * attenuation; 

            float cos_theta = max(dot(H, V), 0.0);
            vec3 F  = fresnel_schlick(cos_theta, F0);

            float NDF = distribution_ggx(N, H, roughness);       
            float G   = geometry_smith(N, V, L, roughness);  

            vec3 numerator    = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
            vec3 specular     = numerator / denominator;  

            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS);
            kD *= 1.0 - metallic;	

            float NdotL = max(dot(N, L), 0.0);                
            Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
        }
        
        vec3 F = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);
        
        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;	

        vec3 irradiance = texture(cubemap_irradiance, N).rgb;
        vec3 diffuse    = irradiance * albedo;
        
        //cubemap_prefilter
        //texture_brdf_lut

        // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefiltered_color = textureLod(cubemap_prefilter, R,  roughness * MAX_REFLECTION_LOD).rgb;    
        vec2 brdf  = texture(texture_brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefiltered_color * (F * brdf.x + brdf.y);

        vec3 ambient    = (kD * diffuse + specular) * ao; 

        vec3 color = ambient + Lo;
        //return irradiance;
        return color;
    }

    vec3 get_normal_from_map()
    {
        vec3 tangent_normal = texture(texture_normal, fs_in.uv).xyz * 2.0 - 1.0;

        vec3 Q1  = dFdx(fs_in.frag_pos);
        vec3 Q2  = dFdy(fs_in.frag_pos);
        vec2 st1 = dFdx(fs_in.uv);
        vec2 st2 = dFdy(fs_in.uv);

        vec3 N   = normalize(fs_in.norm);
        vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
        vec3 B  = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);

        return normalize(TBN * tangent_normal);
    }

    void main()
    {
        vec3 albedo = gamma_correct(solid_albedo, 1.0/gamma);
        vec3 normal = normalize(fs_in.norm);
        float metallic = solid_metallic;
        float roughness = solid_roughness;
        float ao = solid_ao;

        if(use_textures > 0)
        {
            albedo = gamma_correct(texture(texture_albedo, fs_in.uv).rgb, 1.0/gamma);
            normal = get_normal_from_map(); //normalize(fs_in.norm); 
            metallic  = texture(texture_metallic, fs_in.uv).r;
            roughness = texture(texture_roughness, fs_in.uv).r;
            ao        = texture(texture_ao, fs_in.uv).r;
        }

        vec3 color = pbr(albedo, normal, metallic, roughness, ao);
        frag_color = vec4(color, 1.0);
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