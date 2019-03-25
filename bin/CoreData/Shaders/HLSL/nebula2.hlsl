#include "Uniforms.hlsl"
#include "Samplers.hlsl"
#include "Transform.hlsl"
#include "classicnoise4D.hlsl"

#ifdef COMPILEPS
	#ifndef D3D11
	uniform float3 cNebularColor;
	uniform float3 cNebularOffset;
	uniform float cNebularScale;
	uniform float cNebularIntensity;
	uniform float cNebularFalloff;
	#else
	cbuffer CustomPS
	{
		float3 cNebularColor;
		float3 cNebularOffset;
		float cNebularScale;
		float cNebularIntensity;
		float cNebularFalloff;
	}
	#endif
	float noise_nebula(float3 p) {
		return cnoise(float4(p, 0));
	}
	
	float fbm(float3 p, int steps, float lacunarity, float gain)
	{
		float3 sum = 0.0;
		float3 pp = p;
		float amp = 1.0;
		for (int i = 0; i < steps; i++) {
			sum += noise_nebula(pp) * amp;
			amp *= gain;
			pp *= lacunarity;
		}
		return sum;
	}	
#endif

void VS(float4 iPos : POSITION,
    float3 iNormal : NORMAL,    
    out float3 vPos : TEXCOORD0,   
	out float3 vNormal : TEXCOORD1,
    out float4 oPos : OUTPOSITION)
{
    float4x3 modelMatrix = iModelMatrix;
    float3 worldPos = GetWorldPos(modelMatrix);
    oPos = GetClipPos(worldPos);
	vPos = worldPos;
	vNormal = GetWorldNormal(modelMatrix);
}

void PS(
    float3 vPos : TEXCOORD0,
	float3 vNormal : TEXCOORD1,
    out float4 oColor : OUTCOLOR0)
{	
	const float Kd = 0.5;
	const int octaves = 8;
	const float lacunarity = 2;
	const float gain = 0.5;
	const float wispyness = 2.0;
	const float edgeFallOff = 4.0;
	const float distFallOff = 1.0;
	const float mindistfalloff = 1000.0;
	const float maxdistfalloff = 2000.0;
	const float3 lightColor = float3(1.0, 1.0, 1.0);
	const float3 Color = float3(0.8, 0.5, 0.7);
	
	float3 posn = normalize(vPos) * cNebularScale + cNebularOffset;
	float opac = fbm(float3(
		posn.x + wispyness * fbm(posn/10.0, 3, 5.0, 0.5),
		posn.y + wispyness * fbm(posn/10.0, 3, 5.0, 0.5),
		posn.z + wispyness * fbm(posn/10.0, 3, 5.0, 0.5)
		), octaves, lacunarity, gain);
	opac = smoothstep(-1, 1, opac);
	float3 view_dir = vPos - cCameraPosPS;
	float cosx = dot(vNormal, normalize(-view_dir));
	//Falloff near edge of sphere
	opac *= pow(abs(cosx), edgeFallOff);
	//Falloff with distance
	//float reldist = smoothstep(mindistfalloff, maxdistfalloff, length(view_dir)); 
	//opac *= pow(1.0 - reldist, distFallOff); 
	//light source is from camera
	float illuminance = saturate(cosx);
	float3 CL = lightColor * illuminance;
	oColor.rgb = Kd * opac * cNebularColor * Color * CL;
	oColor.a = opac - 0.1;
}
