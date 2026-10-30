struct VertexOutput
{
    float4 position : SV_Position;
};

VertexOutput VSMain(float3 position : POSITION)
{
    VertexOutput output;
    output.position = float4(position, 1.0f);
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
    return float4(1.0f, 0.0f, 0.0f, 0.5f);
}

[numthreads(1, 1, 1)]
void CSMain(uint3 threadId : SV_DispatchThreadID)
{
}
