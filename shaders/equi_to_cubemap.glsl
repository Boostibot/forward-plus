#version 330 core

#ifdef FRAG
    out vec4 FragColor;
    in vec3 WorldPos;

    uniform sampler2D equirectangularMap;
    uniform float gamma;

    const vec2 invAtan = vec2(0.1591, 0.3183);
    vec2 SampleSphericalMap(vec3 v)
    {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        uv += 0.5;
        return uv;
    }

    vec3 gamma_correct(vec3 color, float gamma)
    {
        return pow(color, vec3(1.0 / gamma));
    }

    void main()
    {		
        vec2 uv = SampleSphericalMap(normalize(WorldPos));
        vec3 color = texture(equirectangularMap, uv).rgb;
        color = gamma_correct(color, 1.0/gamma);
        FragColor = vec4(color, 1.0);
    }
#endif 

#ifdef VERT
    layout (location = 0) in vec3 pos;

    out vec3 WorldPos;

    uniform mat4 projection;
    uniform mat4 view;

    void main()
    {
        WorldPos = pos;
        gl_Position =  projection * view * vec4(WorldPos, 1.0);
    }
#endif