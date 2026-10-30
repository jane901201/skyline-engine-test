struct VSInput
{
    float3 position : POSITION;
    uint groupId : GROUPID; //TODO:進 Unigine 要改成 TEXCOORD
};

struct VSOutput
{
    float4 position : SV_Position;
    nointerpolation uint groupId : TEXCOORD0;
};

cbuffer HighlightConstants : register(b0)
{
    uint selectedGroupId;
    
    float3 highlightColor;
    
    float3 baseColor;
    float padding;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    output.position = float4(input.position, 1.0f);
    
    output.groupId = input.groupId;
    
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET{

}