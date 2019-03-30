#include "Uniforms.hlsl"
#include "Samplers.hlsl"
#include "Transform.hlsl"
#include "ScreenPos.hlsl"
#include "Lighting.hlsl"
#include "Fog.hlsl"

void VS(float4 iPos : POSITION,
    #if !defined(BILLBOARD) && !defined(TRAILFACECAM)
        float3 iNormal : NORMAL,
    #endif
    #ifndef NOUV
        float2 iTexCoord : TEXCOORD0,
    #endif
    #ifdef VERTEXCOLOR
        float4 iColor : COLOR0,
    #endif
    #if defined(LIGHTMAP) || defined(AO)
        float2 iTexCoord2 : TEXCOORD1,
    #endif
    //#if (defined(NORMALMAP) || defined(TRAILFACECAM) || defined(TRAILBONE)) && !defined(BILLBOARD) && !defined(DIRBILLBOARD)
    //    float4 iTangent : TANGENT,
    //#endif
    #ifdef SKINNED
        float4 iBlendWeights : BLENDWEIGHT,
        int4 iBlendIndices : BLENDINDICES,
    #endif
    #ifdef INSTANCED
        float4x3 iModelInstance : TEXCOORD4,
    #endif
    #if defined(BILLBOARD) || defined(DIRBILLBOARD)
        float2 iSize : TEXCOORD1,
    #endif
    
	out float4 oLocalPos : TEXCOORD0,
	out float3 oLocalNormal : TEXCOORD3,
    
    out float3 oNormal : TEXCOORD1,
    out float4 oWorldPos : TEXCOORD2,
    #ifdef PERPIXEL
        #ifdef SHADOW
            out float4 oShadowPos[NUMCASCADES] : TEXCOORD4,
        #endif
        #ifdef SPOTLIGHT
            out float4 oSpotPos : TEXCOORD5,
        #endif
        #ifdef POINTLIGHT
            out float3 oCubeMaskVec : TEXCOORD5,
        #endif
    #else
        out float3 oVertexLight : TEXCOORD4,
        out float4 oScreenPos : TEXCOORD5,
        #ifdef ENVCUBEMAP
            out float3 oReflectionVec : TEXCOORD6,
        #endif
        #if defined(LIGHTMAP) || defined(AO)
            out float2 oTexCoord2 : TEXCOORD7,
        #endif
    #endif
    #ifdef VERTEXCOLOR
        out float4 oColor : COLOR0,
    #endif
    #if defined(D3D11) && defined(CLIPPLANE)
        out float oClip : SV_CLIPDISTANCE0,
    #endif
    out float4 oPos : OUTPOSITION)
{
    // Define a 0,0 UV coord if not expected from the vertex data
    //#ifdef NOUV
    //float2 iTexCoord = float2(0.0, 0.0);
    //#endif

    float4x3 modelMatrix = iModelMatrix;
    float3 worldPos = GetWorldPos(modelMatrix);
    oPos = GetClipPos(worldPos);
    oNormal = GetWorldNormal(modelMatrix);
    oWorldPos = float4(worldPos, GetDepth(oPos));

    #if defined(D3D11) && defined(CLIPPLANE)
        oClip = dot(oPos, cClipPlane);
    #endif

    #ifdef VERTEXCOLOR
        oColor = iColor;
    #endif

    oLocalPos = iPos;
	oLocalNormal = iNormal;
    
    #ifdef PERPIXEL
        // Per-pixel forward lighting
        float4 projWorldPos = float4(worldPos.xyz, 1.0);

        #ifdef SHADOW
            // Shadow projection: transform from world space to shadow space
            GetShadowPos(projWorldPos, oNormal, oShadowPos);
        #endif

        #ifdef SPOTLIGHT
            // Spotlight projection: transform from world space to projector texture coordinates
            oSpotPos = mul(projWorldPos, cLightMatrices[0]);
        #endif

        #ifdef POINTLIGHT
            oCubeMaskVec = mul(worldPos - cLightPos.xyz, (float3x3)cLightMatrices[0]);
        #endif
    #else
        // Ambient & per-vertex lighting
        #if defined(LIGHTMAP) || defined(AO)
            // If using lightmap, disregard zone ambient light
            // If using AO, calculate ambient in the PS
            oVertexLight = float3(0.0, 0.0, 0.0);
            oTexCoord2 = iTexCoord2;
        #else
            oVertexLight = GetAmbient(GetZonePos(worldPos));
        #endif

        #ifdef NUMVERTEXLIGHTS
            for (int i = 0; i < NUMVERTEXLIGHTS; ++i)
                oVertexLight += GetVertexLight(i, worldPos, oNormal) * cVertexLights[i * 3].rgb;
        #endif
        
        oScreenPos = GetScreenPos(oPos);

        #ifdef ENVCUBEMAP
            oReflectionVec = worldPos - cCameraPos;
        #endif
    #endif
}

#define TriplanarSample2D(map,pos,weight)	(Sample2D(map,pos.xy)*weight.z+Sample2D(map,pos.yz)*weight.x+Sample2D(map,pos.xz)*weight.y)

// Reoriented Normal Mapping for Unity3d
// http://discourse.selfshadow.com/t/blending-in-detail/21/18

float3 rnmBlendUnpacked(float3 n1, float3 n2)
{
    n1 += float3( 0,  0, 1);
    n2 *= float3(-1, -1, 1);
    return n1*dot(n1, n2)/n1.z - n2;
}

void PS(
	float4 iLocalPos : TEXCOORD0,
	float3 iLocalNormal : TEXCOORD3,
    float3 iNormal : TEXCOORD1,
    float4 iWorldPos : TEXCOORD2,
    #ifdef PERPIXEL
        #ifdef SHADOW
            float4 iShadowPos[NUMCASCADES] : TEXCOORD4,
        #endif
        #ifdef SPOTLIGHT
            float4 iSpotPos : TEXCOORD5,
        #endif
        #ifdef POINTLIGHT
            float3 iCubeMaskVec : TEXCOORD5,
        #endif
    #else
        float3 iVertexLight : TEXCOORD4,
        float4 iScreenPos : TEXCOORD5,
        #ifdef ENVCUBEMAP
            float3 iReflectionVec : TEXCOORD6,
        #endif
        #if defined(LIGHTMAP) || defined(AO)
            float2 iTexCoord2 : TEXCOORD7,
        #endif
    #endif
    #ifdef VERTEXCOLOR
        float4 iColor : COLOR0,
    #endif
    #if defined(D3D11) && defined(CLIPPLANE)
        float iClip : SV_CLIPDISTANCE0,
    #endif
    #ifdef PREPASS
        out float4 oDepth : OUTCOLOR1,
    #endif
    #ifdef DEFERRED
        out float4 oAlbedo : OUTCOLOR1,
        out float4 oNormal : OUTCOLOR2,
        out float4 oDepth : OUTCOLOR3,
    #endif
    out float4 oColor : OUTCOLOR0)
{
	//triplanar weight
	float3 weights = abs(iLocalNormal);  // calculate the weights of the three projected textures
    float sum = weights.x + weights.y + weights.z;  // let all three axes/weights add up to 1.0
    weights.x /= sum;
    weights.y /= sum;
    weights.z /= sum;

    // Get material diffuse albedo
    #ifdef DIFFMAP
        float4 diffInput = TriplanarSample2D(DiffMap, iLocalPos, weights);//Sample2D(DiffMap, iTexCoord.xy);
        #ifdef ALPHAMASK
            if (diffInput.a < 0.5)
                discard;
        #endif
        float4 diffColor = cMatDiffColor * diffInput;
    #else
        float4 diffColor = cMatDiffColor;
    #endif

    #ifdef VERTEXCOLOR
        diffColor *= iColor;
    #endif

    // Get material specular albedo
    #ifdef SPECMAP
        float3 specColor = cMatSpecColor.rgb * TriplanarSample2D(SpecMap, iLocalPos, weights).rgb;//Sample2D(SpecMap, iTexCoord.xy).rgb;
    #else
        float3 specColor = cMatSpecColor.rgb;
    #endif

    // Get normal
    #ifdef NORMALMAP
		// medium.com/@bgolus/normal-mapping-for-a-triplanar-shader-10bf39dca05a
		// Reoriented Normal Mapping blend

		// Triplanar uvs
		float2 uvX = iLocalPos.zy; // x facing plane
		float2 uvY = iLocalPos.xz; // y facing plane
		float2 uvZ = iLocalPos.xy; // z facing plane
		
		// Tangent space normal maps
		float3 tnormalX = DecodeNormal(Sample2D(NormalMap, uvX));
		float3 tnormalY = DecodeNormal(Sample2D(NormalMap, uvY));
		float3 tnormalZ = DecodeNormal(Sample2D(NormalMap, uvZ));
		
		// Get absolute value of normal to ensure positive tangent "z" for blend
		float3 absVertNormal = abs(iLocalNormal);
		
		// Swizzle world normals to match tangent space and apply RNM blend
		tnormalX = rnmBlendUnpacked(float3(iLocalNormal.zy, absVertNormal.x), tnormalX);
		tnormalY = rnmBlendUnpacked(float3(iLocalNormal.xz, absVertNormal.y), tnormalY);
		tnormalZ = rnmBlendUnpacked(float3(iLocalNormal.xy, absVertNormal.z), tnormalZ);
		
		// Get the sign (-1 or 1) of the surface normal
		float3 axisSign = sign(iLocalNormal);
		
		// Reapply sign to Z
		tnormalX.z *= axisSign.x;
		tnormalY.z *= axisSign.y;
		tnormalZ.z *= axisSign.z;
		
		// Triblend normals and add to world normal
		float3 normal = normalize(
			tnormalX.xyz * weights.x +
			tnormalY.xyz * weights.y +
			tnormalZ.xyz * weights.z +
			iLocalNormal
			);
    #else
        float3 normal = normalize(iNormal);
    #endif

    // Get fog factor
    #ifdef HEIGHTFOG
        float fogFactor = GetHeightFogFactor(iWorldPos.w, iWorldPos.y);
    #else
        float fogFactor = GetFogFactor(iWorldPos.w);
    #endif

    #if defined(PERPIXEL)
        // Per-pixel forward lighting
        float3 lightDir;
        float3 lightColor;
        float3 finalColor;

        float diff = GetDiffuse(normal, iWorldPos.xyz, lightDir);

        #ifdef SHADOW
            diff *= GetShadow(iShadowPos, iWorldPos.w);
        #endif

        #if defined(SPOTLIGHT)
            lightColor = iSpotPos.w > 0.0 ? Sample2DProj(LightSpotMap, iSpotPos).rgb * cLightColor.rgb : 0.0;
        #elif defined(CUBEMASK)
            lightColor = SampleCube(LightCubeMap, iCubeMaskVec).rgb * cLightColor.rgb;
        #else
            lightColor = cLightColor.rgb;
        #endif
    
        #ifdef SPECULAR
            float spec = GetSpecular(normal, cCameraPosPS - iWorldPos.xyz, lightDir, cMatSpecColor.a);
            finalColor = diff * lightColor * (diffColor.rgb + spec * specColor * cLightColor.a);
        #else
            finalColor = diff * lightColor * diffColor.rgb;
        #endif

        #ifdef AMBIENT
            finalColor += cAmbientColor.rgb * diffColor.rgb;
            finalColor += cMatEmissiveColor;
            oColor = float4(GetFog(finalColor, fogFactor), diffColor.a);
        #else
            oColor = float4(GetLitFog(finalColor, fogFactor), diffColor.a);
        #endif
    #elif defined(PREPASS)
        // Fill light pre-pass G-Buffer
        float specPower = cMatSpecColor.a / 255.0;

        oColor = float4(normal * 0.5 + 0.5, specPower);
        oDepth = iWorldPos.w;
    #elif defined(DEFERRED)
        // Fill deferred G-buffer
        float specIntensity = specColor.g;
        float specPower = cMatSpecColor.a / 255.0;

        float3 finalColor = iVertexLight * diffColor.rgb;
        #ifdef AO
            // If using AO, the vertex light ambient is black, calculate occluded ambient here
            finalColor += Sample2D(EmissiveMap, iTexCoord2).rgb * cAmbientColor.rgb * diffColor.rgb;
        #endif
        #ifdef ENVCUBEMAP
            finalColor += cMatEnvMapColor * SampleCube(EnvCubeMap, reflect(iReflectionVec, normal)).rgb;
        #endif
        #ifdef LIGHTMAP
            finalColor += Sample2D(EmissiveMap, iTexCoord2).rgb * diffColor.rgb;
        #endif
        #ifdef EMISSIVEMAP
            finalColor += cMatEmissiveColor * TriplanarSample2D(EmissiveMap, iLocalPos, weights).rgb;//Sample2D(EmissiveMap, iTexCoord.xy).rgb;
        #else
            finalColor += cMatEmissiveColor;
        #endif

        oColor = float4(GetFog(finalColor, fogFactor), 1.0);
        oAlbedo = fogFactor * float4(diffColor.rgb, specIntensity);
        oNormal = float4(normal * 0.5 + 0.5, specPower);
        oDepth = iWorldPos.w;
    #else
        // Ambient & per-vertex lighting
        float3 finalColor = iVertexLight * diffColor.rgb;
        #ifdef AO
            // If using AO, the vertex light ambient is black, calculate occluded ambient here
            finalColor += Sample2D(EmissiveMap, iTexCoord2).rgb * cAmbientColor.rgb * diffColor.rgb;
        #endif

        #ifdef MATERIAL
            // Add light pre-pass accumulation result
            // Lights are accumulated at half intensity. Bring back to full intensity now
            float4 lightInput = 2.0 * Sample2DProj(LightBuffer, iScreenPos);
            float3 lightSpecColor = lightInput.a * lightInput.rgb / max(GetIntensity(lightInput.rgb), 0.001);

            finalColor += lightInput.rgb * diffColor.rgb + lightSpecColor * specColor;
        #endif

        #ifdef ENVCUBEMAP
            finalColor += cMatEnvMapColor * SampleCube(EnvCubeMap, reflect(iReflectionVec, normal)).rgb;
        #endif
        #ifdef LIGHTMAP
            finalColor += Sample2D(EmissiveMap, iTexCoord2).rgb * diffColor.rgb;
        #endif
        #ifdef EMISSIVEMAP
            finalColor += cMatEmissiveColor * TriplanarSample2D(EmissiveMap, iLocalPos, weights).rgb;//Sample2D(EmissiveMap, iTexCoord.xy).rgb;
        #else
            finalColor += cMatEmissiveColor;
        #endif

        oColor = float4(GetFog(finalColor, fogFactor), diffColor.a);
    #endif
}
