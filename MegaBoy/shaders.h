#pragma once

namespace shaders
{
    inline constexpr const char* regularVertexShader = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
   	gl_Position = vec4(aPos, 1.0);
	TexCoord = aTexCoord;
}
)";

   inline constexpr const char* regulaFragmentShader = R"(
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
)";

    inline constexpr const char* lcd1xVertexShader = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec4 TEX0;

void main()
{
   gl_Position = vec4(aPos, 1.0);
	TexCoord = aTexCoord;
   TEX0.xy = TexCoord.xy * 1.0001;
}
)";

    inline constexpr const char* lcd1xFragmentShader = R"(
#version 330 core

uniform vec2 TextureSize;
uniform sampler2D Texture;
uniform float alpha;

in vec4 TEX0;
out vec4 FragColor;

#define BRIGHTEN_SCANLINES 16.0
#define BRIGHTEN_LCD 12.0

// Magic Numbers
#define PI 3.141592654

void main()
{
   // Generate LCD grid effect
   // > Note the 0.25 pixel offset -> required to ensure that
   //   scanlines occur *between* pixels
   vec2 angle = 2.0 * PI * ((TEX0.xy * TextureSize.xy) - 0.25);

   float yfactor = (BRIGHTEN_SCANLINES + sin(angle.y)) / (BRIGHTEN_SCANLINES + 1.0);
   float xfactor = (BRIGHTEN_LCD + sin(angle.x)) / (BRIGHTEN_LCD + 1.0);

   // Get colour sample
   vec3 colour = texture(Texture, TEX0.xy).rgb;

   // Apply LCD grid effect
   colour.rgb = yfactor * xfactor * colour.rgb;

   FragColor = vec4(colour.rgb, alpha);
} 
)";

    inline constexpr const char* omniscaleVertexShader = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec4 COL0;
out vec4 TEX0;

in vec4 COLOR;

uniform vec2 TextureSize;
uniform vec2 InputSize;

// vertex compatibility #defines
#define vTexCoord TEX0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize)

void main()
{
    gl_Position = vec4(aPos, 1.0);
	TexCoord = aTexCoord;
    COL0 = COLOR;
    TEX0.xy = TexCoord.xy;
}
)";

    inline constexpr const char* omniscaleFragmentShader = R"(
/*
MIT License

Copyright (c) 2015-2016 Lior Halphon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#version 330 core

uniform vec2 OutputSize;
uniform vec2 TextureSize;
uniform vec2 InputSize;
uniform float alpha;
uniform sampler2D Texture;

out vec4 FragColor;
in vec4 TEX0;

// fragment compatibility #defines
#define Source Texture
#define vTexCoord TEX0.xy

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define outsize vec4(OutputSize, 1.0 / OutputSize)

/* We use the same colorspace as the HQ algorithms. */
vec3 rgb_to_hq_colospace(vec4 rgb)
{
    return vec3( 0.250 * rgb.r + 0.250 * rgb.g + 0.250 * rgb.b,
                 0.250 * rgb.r - 0.000 * rgb.g - 0.250 * rgb.b,
                -0.125 * rgb.r + 0.250 * rgb.g - 0.125 * rgb.b);
}


bool is_different(vec4 a, vec4 b)
{
    vec3 diff = abs(rgb_to_hq_colospace(a) - rgb_to_hq_colospace(b));
    return diff.x > 0.125 || diff.y > 0.027 || diff.z > 0.031;
}

#define P(m, r) ((pattern & (m)) == (r))

#define uResolution outsize.xy
#define textureDimensions SourceSize.xy

vec4 scale(sampler2D image, vec2 coord)
{
    // o = offset, the width of a pixel
    vec2 o = 1.0 / textureDimensions;
    vec2 texCoord = coord;

    /* We always calculate the top left quarter.  If we need a different quarter, we flip our co-ordinates */

    // p = the position within a pixel [0...1]
    vec2 p = fract(texCoord * textureDimensions);

    if (p.x > 0.5) {
        o.x = -o.x;
        p.x = 1.0 - p.x;
    }
    if (p.y > 0.5) {
        o.y = -o.y;
        p.y = 1.0 - p.y;
    }

    vec4 w0 = texture(image, texCoord + vec2( -o.x, -o.y));
    vec4 w1 = texture(image, texCoord + vec2(    0, -o.y));
    vec4 w2 = texture(image, texCoord + vec2(  o.x, -o.y));
    vec4 w3 = texture(image, texCoord + vec2( -o.x,    0));
    vec4 w4 = texture(image, texCoord + vec2(    0,    0));
    vec4 w5 = texture(image, texCoord + vec2(  o.x,    0));
    vec4 w6 = texture(image, texCoord + vec2( -o.x,  o.y));
    vec4 w7 = texture(image, texCoord + vec2(    0,  o.y));
    vec4 w8 = texture(image, texCoord + vec2(  o.x,  o.y));

    int pattern = 0;
    if (is_different(w0, w4)) pattern |= 1 << 0;
    if (is_different(w1, w4)) pattern |= 1 << 1;
    if (is_different(w2, w4)) pattern |= 1 << 2;
    if (is_different(w3, w4)) pattern |= 1 << 3;
    if (is_different(w5, w4)) pattern |= 1 << 4;
    if (is_different(w6, w4)) pattern |= 1 << 5;
    if (is_different(w7, w4)) pattern |= 1 << 6;
    if (is_different(w8, w4)) pattern |= 1 << 7;

    if ((P(0xbf,0x37) || P(0xdb,0x13)) && is_different(w1, w5))
        return mix(w4, w3, 0.5 - p.x);
    if ((P(0xdb,0x49) || P(0xef,0x6d)) && is_different(w7, w3))
        return mix(w4, w1, 0.5 - p.y);
    if ((P(0x0b,0x0b) || P(0xfe,0x4a) || P(0xfe,0x1a)) && is_different(w3, w1))
        return w4;
    if ((P(0x6f,0x2a) || P(0x5b,0x0a) || P(0xbf,0x3a) || P(0xdf,0x5a) ||
         P(0x9f,0x8a) || P(0xcf,0x8a) || P(0xef,0x4e) || P(0x3f,0x0e) ||
         P(0xfb,0x5a) || P(0xbb,0x8a) || P(0x7f,0x5a) || P(0xaf,0x8a) ||
         P(0xeb,0x8a)) && is_different(w3, w1))
        return mix(w4, mix(w4, w0, 0.5 - p.x), 0.5 - p.y);
    if (P(0x0b,0x08))
        return mix(mix(w0 * 0.375 + w1 * 0.25 + w4 * 0.375, w4 * 0.5 + w1 * 0.5, p.x * 2.0), w4, p.y * 2.0);
    if (P(0x0b,0x02))
        return mix(mix(w0 * 0.375 + w3 * 0.25 + w4 * 0.375, w4 * 0.5 + w3 * 0.5, p.y * 2.0), w4, p.x * 2.0);
    if (P(0x2f,0x2f)) {
        float dist = length(p - vec2(0.5));
        float pixel_size = length(1.0 / (uResolution / textureDimensions));
        if (dist < 0.5 - pixel_size / 2.) {
            return w4;
        }
        vec4 r;
        if (is_different(w0, w1) || is_different(w0, w3)) {
            r = mix(w1, w3, p.y - p.x + 0.5);
        }
        else {
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        }

        if (dist > 0.5 + pixel_size / 2.) {
            return r;
        }
        return mix(w4, r, (dist - 0.5 + pixel_size / 2.) / pixel_size);
    }
    if (P(0xbf,0x37) || P(0xdb,0x13)) {
        float dist = p.x - 2.0 * p.y;
        float pixel_size = length(1.0 / (uResolution / textureDimensions)) * sqrt(5.);
        if (dist > pixel_size / 2.) {
            return w1;
        }
        vec4 r = mix(w3, w4, p.x + 0.5);
        if (dist < -pixel_size / 2.) {
            return r;
        }
        return mix(r, w1, (dist + pixel_size / 2.) / pixel_size);
    }
    if (P(0xdb,0x49) || P(0xef,0x6d)) {
        float dist = p.y - 2.0 * p.x;
        float pixel_size = length(1.0 / (uResolution / textureDimensions)) * sqrt(5.);
        if (p.y - 2.0 * p.x > pixel_size / 2.) {
            return w3;
        }
        vec4 r = mix(w1, w4, p.x + 0.5);
        if (dist < -pixel_size / 2.) {
            return r;
        }
        return mix(r, w3, (dist + pixel_size / 2.) / pixel_size);
    }
    if (P(0xbf,0x8f) || P(0x7e,0x0e)) {
        float dist = p.x + 2.0 * p.y;
        float pixel_size = length(1.0 / (uResolution / textureDimensions)) * sqrt(5.);

        if (dist > 1.0 + pixel_size / 2.) {
            return w4;
        }

        vec4 r;
        if (is_different(w0, w1) || is_different(w0, w3)) {
            r = mix(w1, w3, p.y - p.x + 0.5);
        }
        else {
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        }

        if (dist < 1.0 - pixel_size / 2.) {
            return r;
        }

        return mix(r, w4, (dist + pixel_size / 2. - 1.0) / pixel_size);

    }

    if (P(0x7e,0x2a) || P(0xef,0xab)) {
        float dist = p.y + 2.0 * p.x;
        float pixel_size = length(1.0 / (uResolution / textureDimensions)) * sqrt(5.);

        if (p.y + 2.0 * p.x > 1.0 + pixel_size / 2.) {
            return w4;
        }

        vec4 r;

        if (is_different(w0, w1) || is_different(w0, w3)) {
            r = mix(w1, w3, p.y - p.x + 0.5);
        }
        else {
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        }

        if (dist < 1.0 - pixel_size / 2.) {
            return r;
        }

        return mix(r, w4, (dist + pixel_size / 2. - 1.0) / pixel_size);
    }

    if (P(0x1b,0x03) || P(0x4f,0x43) || P(0x8b,0x83) || P(0x6b,0x43))
        return mix(w4, w3, 0.5 - p.x);

    if (P(0x4b,0x09) || P(0x8b,0x89) || P(0x1f,0x19) || P(0x3b,0x19))
        return mix(w4, w1, 0.5 - p.y);

    if (P(0xfb,0x6a) || P(0x6f,0x6e) || P(0x3f,0x3e) || P(0xfb,0xfa) ||
        P(0xdf,0xde) || P(0xdf,0x1e))
        return mix(w4, w0, (1.0 - p.x - p.y) / 2.0);

    if (P(0x4f,0x4b) || P(0x9f,0x1b) || P(0x2f,0x0b) ||
        P(0xbe,0x0a) || P(0xee,0x0a) || P(0x7e,0x0a) || P(0xeb,0x4b) ||
        P(0x3b,0x1b)) {
        float dist = p.x + p.y;
        float pixel_size = length(1.0 / (uResolution / textureDimensions));

        if (dist > 0.5 + pixel_size / 2.) {
            return w4;
        }

        vec4 r;
        if (is_different(w0, w1) || is_different(w0, w3)) {
            r = mix(w1, w3, p.y - p.x + 0.5);
        }
        else {
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        }

        if (dist < 0.5 - pixel_size / 2.) {
            return r;
        }

        return mix(r, w4, (dist + pixel_size / 2. - 0.5) / pixel_size);
    }

    if (P(0x0b,0x01))
        return mix(mix(w4, w3, 0.5 - p.x), mix(w1, (w1 + w3) / 2.0, 0.5 - p.x), 0.5 - p.y);

    if (P(0x0b,0x00))
        return mix(mix(w4, w3, 0.5 - p.x), mix(w1, w0, 0.5 - p.x), 0.5 - p.y);

    float dist = p.x + p.y;
    float pixel_size = length(1.0 / (uResolution / textureDimensions));

    if (dist > 0.5 + pixel_size / 2.)
        return w4;

    /* We need more samples to "solve" this diagonal */
    vec4 x0 = texture(image, texCoord + vec2( -o.x * 2.0, -o.y * 2.0));
    vec4 x1 = texture(image, texCoord + vec2( -o.x      , -o.y * 2.0));
    vec4 x2 = texture(image, texCoord + vec2(  0.0      , -o.y * 2.0));
    vec4 x3 = texture(image, texCoord + vec2(  o.x      , -o.y * 2.0));
    vec4 x4 = texture(image, texCoord + vec2( -o.x * 2.0, -o.y      ));
    vec4 x5 = texture(image, texCoord + vec2( -o.x * 2.0,  0.0      ));
    vec4 x6 = texture(image, texCoord + vec2( -o.x * 2.0,  o.y      ));

    if (is_different(x0, w4)) pattern |= 1 << 8;
    if (is_different(x1, w4)) pattern |= 1 << 9;
    if (is_different(x2, w4)) pattern |= 1 << 10;
    if (is_different(x3, w4)) pattern |= 1 << 11;
    if (is_different(x4, w4)) pattern |= 1 << 12;
    if (is_different(x5, w4)) pattern |= 1 << 13;
    if (is_different(x6, w4)) pattern |= 1 << 14;

    int diagonal_bias = -7;
    while (pattern != 0) {
        diagonal_bias += pattern & 1;
        pattern >>= 1;
    }

    if (diagonal_bias <=  0) {
        vec4 r = mix(w1, w3, p.y - p.x + 0.5);
        if (dist < 0.5 - pixel_size / 2.) {
            return r;
        }
        return mix(r, w4, (dist + pixel_size / 2. - 0.5) / pixel_size);
    }
    
    return w4;
}

void main()
{
	vec4 textColor = scale(Source, vTexCoord);
    FragColor = vec4(textColor.rgb, alpha);
} 
)";
}