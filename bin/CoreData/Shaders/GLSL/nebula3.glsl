#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"

varying vec2 vTexCoord;
varying vec3 wPos;
varying vec3 wNormal;


void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
	vTexCoord = GetTexCoord(iTexCoord);
    wPos = worldPos;
	wNormal = GetWorldNormal(modelMatrix);
}

void PS()
{
	vec4 diffInput = texture2D(sDiffMap, vTexCoord.xy);
	float cosx = dot(wNormal, normalize(cCameraPosPS-wPos));
	float a = pow(abs(cosx), 4.0);
    gl_FragColor = vec4(diffInput.rgb, diffInput.a * a);
}
