#include "particle_common.hlsli"

// 宣告供渲染階段唯讀使用的粒子 StructuredBuffer 並綁定至 t0
StructuredBuffer<Particle> particles : register(t0);

// 宣告相機與粒子渲染參數的常數緩衝區並綁定至 b0
cbuffer CameraConstants : register(b0)
{
    // 儲存 View Matrix 與 Projection Matrix 合併後的矩陣
    row_major float4x4 viewProjection;

    // 儲存相機在世界空間中的右方向
    float3 cameraRight;

    // 儲存 Billboard 粒子的尺寸
    float particleSize;

    // 儲存相機在世界空間中的上方向
    float3 cameraUp;

    // 保留欄位以符合 Constant Buffer 的記憶體對齊需求
    float padding;

    // 儲存所有粒子使用的 RGBA 顏色
    float4 particleColor;
};

// 定義 Vertex Shader 傳遞給 Pixel Shader 的資料
struct VSOutput
{
    // 儲存經過座標轉換後的 Clip Space 位置
    float4 position : SV_POSITION;

    // 儲存傳遞給 Pixel Shader 的 UV 座標
    float2 uv : TEXCOORD0;
};

// Vertex Shader 的主要進入點
VSOutput VSMain(
    // 取得目前 Instance 中正在處理的頂點編號
    uint vertexID : SV_VertexID,

    // 取得目前正在繪製的 Instance 編號
    uint instanceID : SV_InstanceID)
{
    // 建立 Vertex Shader 的輸出資料
    VSOutput output;

    // 建立兩個三角形所需的六個 Billboard 頂點座標
    float2 corners[6] =
    {
        // 第一個三角形的左下角
        float2(-1.0f, -1.0f),

        // 第一個三角形的左上角
        float2(-1.0f, 1.0f),

        // 第一個三角形的右上角
        float2(1.0f, 1.0f),

        // 第二個三角形的左下角
        float2(-1.0f, -1.0f),

        // 第二個三角形的右上角
        float2(1.0f, 1.0f),

        // 第二個三角形的右下角
        float2(1.0f, -1.0f)
    };

    // 建立與六個 Billboard 頂點一一對應的 UV 座標
    float2 uvs[6] =
    {
        // 第一個三角形左下頂點使用左下 UV
        float2(0.0f, 1.0f),

        // 第一個三角形左上頂點使用左上 UV
        float2(0.0f, 0.0f),

        // 第一個三角形右上頂點使用右上 UV
        float2(1.0f, 0.0f),

        // 第二個三角形左下頂點使用左下 UV
        float2(0.0f, 1.0f),

        // 第二個三角形右上頂點使用右上 UV
        float2(1.0f, 0.0f),

        // 第二個三角形右下頂點使用右下 UV
        float2(1.0f, 1.0f)
    };

    // 使用 Instance ID 取得目前要繪製的粒子
    Particle particle =
        particles[instanceID];

    // 使用 Vertex ID 取得目前 Billboard 頂點的區域座標
    float2 corner =
        corners[vertexID];

    // 將粒子中心沿相機 Right 與 Up 方向展開成面向相機的 Billboard
    float3 worldPosition =
        particle.position
        + cameraRight
            * corner.x
            * particleSize
        + cameraUp
            * corner.y
            * particleSize;

    // 將 Billboard 頂點從 World Space 轉換到 Clip Space
    output.position =
        mul(
            float4(worldPosition, 1.0f),
            viewProjection);

    // 將目前頂點對應的 UV 傳遞至 Pixel Shader
    output.uv =
        uvs[vertexID];

    // 回傳完成計算的頂點輸出
    return output;
}

// Pixel Shader 的主要進入點
float4 PSMain(VSOutput input)
    : SV_TARGET
{
    // 將 UV 從 0 到 1 轉換成以中心為原點的負 1 到正 1 範圍
    float2 centeredUV =
        input.uv * 2.0f - 1.0f;

    // 計算目前 Pixel 與 Billboard 中心的距離
    float radius =
        length(centeredUV);

    // 根據距離產生從中心向外逐漸降低的透明度
    float alpha =
        saturate(
            1.0f - radius);

    // 排除圓形範圍外以及幾乎完全透明的 Pixel
    if (alpha <= 0.01f)
    {
        // 中止目前 Pixel 的輸出
        discard;
    }

    // 結合粒子顏色與計算後的透明度輸出最終顏色
    return float4(particleColor.rgb, particleColor.a * alpha);
}
