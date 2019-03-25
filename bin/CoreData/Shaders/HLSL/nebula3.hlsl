#include "Uniforms.hlsl"
#include "Samplers.hlsl"
#include "Transform.hlsl"


void VS(float4 iPos : POSITION,
    float3 iNormal : NORMAL,    
	float2 iTexCoord : TEXCOORD0,
	out float2 oTexCoord : TEXCOORD0,
    out float3 wPos : TEXCOORD1,   
	out float3 wNormal : TEXCOORD2,
    out float4 oPos : OUTPOSITION)
{
    float4x3 modelMatrix = iModelMatrix;
    float3 worldPos = GetWorldPos(modelMatrix);
    oPos = GetClipPos(worldPos);
	wPos = worldPos;
	wNormal = GetWorldNormal(modelMatrix);
	oTexCoord = GetTexCoord(iTexCoord);
}

void PS(
	float2 iTexCoord : TEXCOORD0,
    float3 wPos : TEXCOORD1,
	float3 wNormal : TEXCOORD2,
    out float4 oColor : OUTCOLOR0)
{	
	float4 diffInput = Sample2D(DiffMap, iTexCoord.xy);
	float cosx = dot(wNormal, normalize(cCameraPosPS-wPos));
	float a = pow(abs(cosx), 4.0);
	oColor = float4(diffInput.rgb, diffInput.a * a);
}
