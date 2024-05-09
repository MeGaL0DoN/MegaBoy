#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform float alpha;

void main()
{
	vec4 textColor = texture(texture1, TexCoord);
	FragColor = vec4(textColor.rgb, alpha);
};