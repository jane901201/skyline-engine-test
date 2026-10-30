// 頂點著色器傳遞給像素著色器的資料。
struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// 太陽與相機資料必須使用同一個世界空間方向。
cbuffer SunConstants : register(b0)
{
    row_major float4x4 inverseView;
    row_major float4x4 inverseProjection;
    float3 sunDirection;
    float sunRadius;
    float3 sunColor;
    float sunIntensity;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    static const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    const float2 position = positions[vertexId];

    VSOutput output;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = position * float2(0.5f, -0.5f) + 0.5f;
    return output;
}

float3 GetWorldViewDirection(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 viewPosition = mul(float4(ndc, 1.0f, 1.0f), inverseProjection);
    viewPosition /= viewPosition.w;

    const float3 viewDirection = normalize(viewPosition.xyz);
    return normalize(mul(float4(viewDirection, 0.0f), inverseView).xyz);
}

float4 PSMain(VSOutput input) : SV_Target
{
    const float3 viewDirection = GetWorldViewDirection(input.uv);
    const float alignment = dot(viewDirection, normalize(sunDirection));
    const float discEdge = cos(sunRadius);
    const float softEdge = cos(sunRadius * 1.12f);
    const float glowEdge = cos(sunRadius * 4.0f);

    const float sunDisc = smoothstep(softEdge, discEdge, alignment);
    const float glow = smoothstep(glowEdge, discEdge, alignment);
    const float3 color = sunColor * sunIntensity * (sunDisc + glow * 0.2f);
    const float alpha = saturate(sunDisc + glow * 0.16f);
    return float4(color, alpha);
}
