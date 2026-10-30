#include "common.hlsl"

// 定義 Vertex Shader 傳給 Pixel Shader 的資料
struct VSOutput
{
    // 儲存頂點的裁切空間位置
    float4 position : SV_POSITION;

    // 儲存垂直方向的天空漸層值
    float gradient : TEXCOORD0;
};

// 根據 Vertex ID 建立 Fullscreen Triangle 的頂點資料
VSOutput VSMain(uint vertexId : SV_VertexID)
{
    // 取得目前 Vertex ID 對應的 Fullscreen Triangle 位置
    const float2 position = GetFullscreenTrianglePosition(vertexId);

    // 建立 Vertex Shader 輸出資料
    VSOutput output;

    // 將二維位置轉換成裁切空間座標
    output.position = float4(position, 0.0f, 1.0f);

    // 將 Y 座標從 [-1, 1] 映射到 [0, 1] 作為天空漸層值
    output.gradient = position.y * 0.5f + 0.5f;

    // 回傳 Vertex Shader 輸出
    return output;
}

// 根據垂直漸層產生天空顏色
float4 PSMain(VSOutput input) : SV_TARGET
{
    // 定義接近地平線的天空顏色
    const float3 horizonColor = float3(0.65f, 0.85f, 1.0f);

    // 定義接近天頂的天空顏色
    const float3 zenithColor = float3(0.08f, 0.35f, 0.85f);

    // 將插值後的漸層值限制在 [0, 1] 範圍
    const float gradient = saturate(input.gradient);

    // 根據漸層值在地平線與天頂顏色之間進行線性插值
    const float3 color = lerp(horizonColor, zenithColor, gradient);

    // 輸出不透明的天空顏色
    return float4(color, 1.0f);
}