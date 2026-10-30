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
float4 FullscreenMain(uint id : SV_VertexID) : SV_Position
{
    return float4(id == 2 ? 3 : -1, id == 1 ? 3 : -1, 0, 1);
}
RWTexture2D<float4> storageTexture : register(u0);
[numthreads(8, 8, 1)]
void FillMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    storageTexture.GetDimensions(width, height);
    if (id.x < width && id.y < height)
        storageTexture[id.xy] = float4(32.0 / 255.0, 64.0 / 255.0, 128.0 / 255.0, 1);
}
