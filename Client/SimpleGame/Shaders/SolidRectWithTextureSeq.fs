#version 330

out vec4 FragColor;

in vec2 v_TexPosition;

uniform sampler2D u_Texture;
uniform float u_TotalSeqX;
uniform float u_TotalSeqY;
uniform float u_CurrSeqX;
uniform float u_CurrSeqY;
uniform vec4 u_Color;
uniform float u_Depth;

void main()
{
	vec2 newTexPos = vec2(v_TexPosition.x, 1-v_TexPosition.y);
	newTexPos.x = u_CurrSeqX/u_TotalSeqX + newTexPos.x/u_TotalSeqX;
	newTexPos.y = u_CurrSeqY/u_TotalSeqY + newTexPos.y/u_TotalSeqY;
	FragColor = texture2D(u_Texture, newTexPos);
    if(FragColor.a == 0)
        gl_FragDepth = 1.0;
    else
        gl_FragDepth = u_Depth;
}
