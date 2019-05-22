#version 330

out vec4 FragColor;

uniform vec4 u_Color;
uniform float u_Depth;

void main()
{
	FragColor = vec4(u_Color.r, u_Color.g, u_Color.b, u_Color.a);
    gl_FragDepth = u_Depth;
    if(FragColor.a == 0)
        gl_FragDepth = 1.0;
    else
        gl_FragDepth = u_Depth;
}
