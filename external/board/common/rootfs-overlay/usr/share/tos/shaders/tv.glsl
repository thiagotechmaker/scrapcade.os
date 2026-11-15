// ScrapCade OS: linhas de varredura de TV de tubo, uma por linha da imagem do jogo. GPLv3.
#if defined(VERTEX)
attribute vec4 VertexCoord;
attribute vec4 TexCoord;
uniform mat4 MVPMatrix;
varying vec2 vTex;
void main() { gl_Position = MVPMatrix * VertexCoord; vTex = TexCoord.xy; }
#elif defined(FRAGMENT)
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D Texture;
uniform vec2 TextureSize;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(Texture, vTex);
    float linha = vTex.y * TextureSize.y;                               // linha da imagem do jogo
    float s = 0.80 + 0.20 * (0.5 + 0.5 * sin(linha * 6.2831853));      // escurece entre as linhas
    gl_FragColor = vec4(c.rgb * s, c.a);
}
#endif
