Texture2D<float4> colorTexture : register(t0);
SamplerState colorSampler : register(s0);
struct VertexOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD; };
VertexOutput VSMain(float2 position : POSITION)
{
    VertexOutput output;
    output.position = float4(position, 0, 1);
    output.uv = float2(position.x * 0.5 + 0.5, 0.5 - position.y * 0.5);
    return output;
}
float4 PSMain(VertexOutput input) : SV_TARGET
{
    return colorTexture.Sample(colorSampler, input.uv);
}
