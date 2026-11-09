// GbtG cel preview for 3ds Max's DirectX Shader material (DX11 viewport). Used by the GbtG Material that
// tools/max/gbtg_tools.ms defines; see docs/INSPECTOR.md, "3ds Max tools".
//
// An approximation of the game's actor look for authoring, not the game's combiner: the colour map is lit through
// the material's cel ramp (RARE_celmap0, a 256x2 ramp sampled with the Lambert term) and gets the soft lighting
// blob (RARE_lightmap0) added, sampled by the view-space normal rather than by UV. The inspector draws the real
// thing after Send to game.

string ParamID = "0x003";

float Script : STANDARDSGLOBAL <
    string UIWidget = "none";
    string ScriptClass = "object";
    string ScriptOrder = "standard";
    string ScriptOutput = "color";
    string Script = "Technique=Main;";
> = 0.8;

float4x4 WorldITXf : WorldInverseTranspose < string UIWidget = "None"; >;
float4x4 WvpXf : WorldViewProjection < string UIWidget = "None"; >;
float4x4 WorldXf : World < string UIWidget = "None"; >;
float4x4 ViewXf : View < string UIWidget = "None"; >;

Texture2D <float4> g_ColourTexture : DiffuseMap <
    string UIName = "Colour Map";
    string ResourceType = "2D";
    int Texcoord = 0;
    int MapChannel = 1;
>;

bool g_UseColourTexture <
    string UIName = "Use Colour Map";
> = true;

float4 g_Colour <
    string UIName = "Colour (no map)";
    string UIWidget = "Color";
> = float4(1.0f, 1.0f, 1.0f, 1.0f);

Texture2D <float4> g_CelTexture <
    string UIName = "Cel Ramp";
    string ResourceType = "2D";
>;

bool g_UseCelTexture <
    string UIName = "Use Cel Ramp";
> = true;

Texture2D <float4> g_LightTexture <
    string UIName = "Lighting Blob";
    string ResourceType = "2D";
>;

bool g_UseLightTexture <
    string UIName = "Use Lighting Blob";
> = true;

float3 g_LightDirection <
    string UIName = "Light Direction (world, toward light)";
> = float3(-0.4f, -0.6f, 0.7f);

float g_RampScale <
    string UIName = "Ramp Brightness";
    string UIWidget = "slider";
    float UIMin = 0.5f;
    float UIMax = 3.0f;
    float UIStep = 0.05f;
> = 1.6f;

float g_LightScale <
    string UIName = "Blob Strength";
    string UIWidget = "slider";
    float UIMin = 0.0f;
    float UIMax = 3.0f;
    float UIStep = 0.05f;
> = 1.0f;

SamplerState g_ColourSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState g_RampSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState g_LightSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct appdata
{
    float4 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV0 : TEXCOORD0;
};

struct vertexOutput
{
    float4 HPosition : SV_Position;
    float2 UV0 : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 ViewNormal : TEXCOORD2;
};

vertexOutput cel_VS(appdata IN)
{
    vertexOutput OUT = (vertexOutput)0;
    OUT.WorldNormal = mul(IN.Normal, (float3x3)WorldITXf);
    OUT.ViewNormal = mul(OUT.WorldNormal, (float3x3)ViewXf);
    OUT.HPosition = mul(float4(IN.Position.xyz, 1.0f), WvpXf);
    OUT.UV0 = IN.UV0;
    return OUT;
}

float4 cel_PS(vertexOutput IN) : SV_Target
{
    const float3 normal = normalize(IN.WorldNormal);
    float4 colour = g_UseColourTexture ? g_ColourTexture.Sample(g_ColourSampler, IN.UV0) : g_Colour;
    // The ramp runs dark to light across U; the Lambert term picks the band.
    const float lambert = saturate(dot(normal, normalize(g_LightDirection)));
    float3 lit = g_UseCelTexture
        ? g_CelTexture.Sample(g_RampSampler, float2(lambert * (254.0f / 256.0f) + (1.0f / 256.0f), 0.25f)).rgb
        : (0.45f + 0.35f * lambert).xxx;
    float3 result = colour.rgb * lit * g_RampScale;
    if (g_UseLightTexture)
    {
        // A sphere-map lookup: the view-space normal's X and Y across the blob.
        const float3 view_normal = normalize(IN.ViewNormal);
        const float2 blob = float2(view_normal.x, -view_normal.y) * 0.5f + 0.5f;
        result += g_LightTexture.Sample(g_LightSampler, blob).rgb * g_LightScale;
    }
    return float4(saturate(result), colour.a);
}

technique11 Main_11 <
    string Script = "Pass=p0;";
>
{
    pass p0 <
        string Script = "Draw=geometry;";
    >
    {
        SetVertexShader(CompileShader(vs_5_0, cel_VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, cel_PS()));
    }
}
