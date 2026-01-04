#define MAX_LIGHTS            16
#define MAX_MATERIALS        512

#define POINT_LIGHT            1
#define SPOT_LIGHT             2
#define DIRECTIONAL_LIGHT     3

#define _WITH_LOCAL_VIEWER_HIGHLIGHTING
#define _WITH_THETA_PHI_CONES
//#define _WITH_REFLECT

struct LIGHT
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular;
    float3 m_vPosition;
    float m_fFalloff;
    float3 m_vDirection;
    float m_fTheta; //cos(m_fTheta)
    float3 m_vAttenuation;
    float m_fPhi; //cos(m_fPhi)
    bool m_bEnable;
    int m_nType;
    float m_fRange;
    float padding;
};

cbuffer cbLights : register(b3)
{
    LIGHT gLights[MAX_LIGHTS];
    float4 gcGlobalAmbientLight;
    int gnLights;
};

const float PI = 3.14159265359;

// D: Normal Distribution Function (GGX)
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// G: Geometry Function (Smith's method with Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// F: Fresnel Equation (Schlick's approximation)
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float4 Lighting(float3 worldPos, float3 N, float3 V, float3 albedo, float metallic, float roughness, float ao)
{
    float3 F0 = lerp(0.04, albedo, metallic);
   
    float3 Lo = float3(0.0, 0.0, 0.0);

    [unroll(MAX_LIGHTS)]
    for (int i = 0; i < gnLights; i++)
    {
        
        if (!gLights[i].m_bEnable)
            continue;
        
        float3 L;
        float attenuation = 1.0;
        	
    	// Directional Light
        if (gLights[i].m_nType == DIRECTIONAL_LIGHT)
        {
            L = normalize(gLights[i].m_vDirection);
            attenuation = 1.0;
        }

        // Point Light
        else if (gLights[i].m_nType == POINT_LIGHT)
            {
            L = normalize(gLights[i].m_vPosition - worldPos);
            
            float distance = length(gLights[i].m_vPosition - worldPos);
           
            if (distance > gLights[i].m_fRange)
               continue;
           
            attenuation = 1.0 / dot(gLights[i].m_vAttenuation, float3(1.0, distance, distance * distance));
            
        }

    	// Spot Light
        else if (gLights[i].m_nType == SPOT_LIGHT)
            {
          
            L = normalize(gLights[i].m_vPosition - worldPos);
         
            float distance = length(gLights[i].m_vPosition - worldPos);
            if (distance > gLights[i].m_fRange)
            	continue;
            float fAlpha = max(dot(-L, gLights[i].m_vDirection), 0.0f);
            float fSpotFactor = pow(max(((fAlpha - gLights[i].m_fPhi) / (gLights[i].m_fTheta - gLights[i].m_fPhi)), 0.0f), gLights[i].m_fFalloff);
            attenuation = fSpotFactor / dot(gLights[i].m_vAttenuation, float3(1.0, distance, distance * distance));
        }

        float3 H = normalize(V + L);
        float3 radiance = gLights[i].m_cDiffuse.rgb * attenuation;
        
    	// Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(saturate(dot(H, V)), F0);
        float3 kD = (1.0 - F) * (1.0 - metallic);
        float NdotL = saturate(dot(N, L));

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * saturate(dot(N, V)) * NdotL + 0.0001;
    	float3 specular = numerator / denominator;
       
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
	// Global Ambient
    float3 ambient = gcGlobalAmbientLight.rgb * albedo * ao;
    float3 color = ambient + Lo;

    return float4(color, 1.0);
}