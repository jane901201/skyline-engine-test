cbuffer TransformCB
{
    row_major float4x4 worldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.normal = input.normal;
    output.uv = input.uv;

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(0.8f, 0.8f, 0.8f, 1.0f);
}
