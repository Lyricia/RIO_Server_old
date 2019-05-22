#version 330

in vec2 v_TexPosition;

out vec4 FragColor;

uniform vec4 u_Color;
uniform float u_Gauge;
uniform float u_Depth;

void main()
{
    vec4 newColor = u_Color;
    if(v_TexPosition.x > u_Gauge)
    {
        newColor = vec4(0, 0, 0, 0);
    }
	FragColor = newColor;
    if(FragColor.a == 0)
        gl_FragDepth = 1.0;
    else
        gl_FragDepth = u_Depth;
}
