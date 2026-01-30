#version 460 core
layout (location = 0) in vec4 a_Vertex; // pos.xy, uv.zw

out vec2 v_UV;

uniform mat4 u_Projection;

void main()
{
    gl_Position = u_Projection * vec4(a_Vertex.xy, 0.0, 1.0);
    v_UV = a_Vertex.zw;
}
