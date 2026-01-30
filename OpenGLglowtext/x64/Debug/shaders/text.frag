#version 460 core
in vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_Text;
uniform vec3 u_Color;
uniform bool u_IsIcon;

void main()
{
    if (u_IsIcon)
    {
        // ICON MODE: Read the full color (RGBA) from the texture
        vec4 sampledColor = texture(u_Text, v_UV);
        
        // Here we can use u_Color to tint the icon, but if it is white (1,1,1), you will see the original colors.
        FragColor = vec4(u_Color, 1.0) * sampledColor;
    }
    else
    {
        float alpha = texture(u_Text, v_UV).r;
        FragColor = vec4(u_Color, alpha);
    }
}
