#version 330

out vec4 FragColor;

in vec2 v_TexPosition;

uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform float u_Depth;

void main()
{
	vec2 newTexPos = vec2(v_TexPosition.x, 1-v_TexPosition.y);
	FragColor = texture2D(u_Texture, newTexPos)*u_Color;
    if(FragColor.a == 0)
        gl_FragDepth = 1.0;
    else
        gl_FragDepth = u_Depth;
}
