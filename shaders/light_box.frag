#version 330 core
//from learnopengl.com
layout (location = 0) out vec4 FragColor;

uniform vec4 lightColor;

void main()
{           
    FragColor = lightColor;
}

