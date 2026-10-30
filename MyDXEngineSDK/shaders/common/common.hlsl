// 全螢幕三角形的頂點著色器輸出資料
struct FullscreenVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// 記憶體配置必須與 CommonPass::CameraConstants 保持一致
cbuffer CameraConstants : register(b0)
{
    row_major float4x4 inverseView;
    row_major float4x4 inverseProjection;
    float3 cameraPosition;
    float cameraPadding;
};

// 根據頂點編號建立覆蓋整個畫面的三角形
FullscreenVertexOutput VSMain(uint vertexId : SV_VertexID)
{
    static const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    const float2 position = positions[vertexId];

    FullscreenVertexOutput output;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = position * float2(0.5f, -0.5f) + 0.5f;
    return output;
}

// 將螢幕 UV 轉換成世界空間的觀察方向
float3 getWorldViewDirection(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 viewPosition = mul(float4(ndc, 1.0f, 1.0f), inverseProjection);
    viewPosition /= viewPosition.w;

    const float3 viewDirection = normalize(viewPosition.xyz);
    return normalize(mul(float4(viewDirection, 0.0f), inverseView).xyz);
}

// 產生 CommonPass 使用的天空漸層背景
float4 PSMain(FullscreenVertexOutput input) : SV_Target
{
    const float3 viewDirection = getWorldViewDirection(input.uv);
    const float3 horizonColor = float3(0.79f, 0.68f, 0.84f);
    const float3 zenithColor = float3(0.27f, 0.37f, 0.66f);

    // 此引擎的世界座標使用 Z 軸作為上方向
    const float height = sqrt(saturate(viewDirection.z));
    const float3 color = lerp(horizonColor, zenithColor, height);

    return float4(color, 1.0f);
}
