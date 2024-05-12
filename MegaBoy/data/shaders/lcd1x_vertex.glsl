#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying 
#define COMPAT_ATTRIBUTE attribute 
#define COMPAT_TEXTURE texture2D
#endif

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;

COMPAT_VARYING vec4 TEX0;

void main()
{
   gl_Position = vec4(aPos, 1.0);
	TexCoord = aTexCoord;
   TEX0.xy = TexCoord.xy * 1.0001;
}