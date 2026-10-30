// 新增 constant buffer 用來控制顏色
cbuffer TriangleConstants : register(b0)
{
    float4 triangleColor;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.position = float4(input.position, 1.0f);
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return triangleColor;
}
