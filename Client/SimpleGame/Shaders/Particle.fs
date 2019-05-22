#version 330

in vec2 v_TexPos;
in float v_Alpha;
in float v_Spark;

out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform float u_Depth;

void main()
{
    FragColor = texture(u_Texture, v_TexPos);//*u_Color;
    FragColor.a *= v_Alpha;
    FragColor *= u_Color * v_Spark;
    gl_FragDepth = u_Depth;
    //FragColor = vec4(1, 1, 1, 1);
}
