#ifndef SKYLINE_COMMON_INCLUDED
#define SKYLINE_COMMON_INCLUDED

// 定義 Fullscreen Triangle 的 Vertex Shader 輸出資料
struct FullscreenVSOutput
{
    // 儲存裁切空間位置
    float4 position : SV_Position;

    // 儲存對應的螢幕 UV 座標
    float2 uv : TEXCOORD0;
};

// 定義 Camera 相關的常數資料並綁定到 b0
cbuffer CameraConstants : register(b0)
{
    // 儲存 View Matrix 的反矩陣
    row_major float4x4 inverseView;

    // 儲存 Projection Matrix 的反矩陣
    row_major float4x4 inverseProjection;
    
    // 儲存 Camera 在 World Space 中的位置
    float3 cameraPosition;

    // 補齊 Constant Buffer 記憶體對齊
    float _cameraPadding;
}

// 根據 Vertex ID 取得 Fullscreen Triangle 的頂點位置
float2 GetFullscreenTrianglePosition(uint vertexId)
{
    // 定義覆蓋整個畫面的三角形三個頂點
    static const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    
    // 回傳目前 Vertex ID 對應的位置
    return positions[vertexId];
}

// 將 Fullscreen Triangle 的 NDC 位置轉換成 Texture UV
float2 GetFullscreenTriangleUV(float2 position)
{
    // 將 [-1, 1] 範圍轉成 [0, 1]，並反轉 Y 軸方向
    return position * float2(0.5f, -0.5f) + 0.5f;
}

// 根據 Vertex ID 建立 Fullscreen Triangle 的 Vertex Shader 輸出
FullscreenVSOutput GetFullscreenTriangleVertex(uint vertexId)
{
    // 建立 Vertex Shader 輸出資料
    FullscreenVSOutput output;
    
    // 取得目前頂點的 NDC 位置
    const float2 position = GetFullscreenTrianglePosition(vertexId);
    
    // 將二維位置轉成裁切空間座標
    output.position = float4(position, 0.0f, 1.0f);
    
    // 計算目前頂點對應的 UV 座標
    output.uv = GetFullscreenTriangleUV(position);
    
    // 回傳建立完成的頂點資料
    return output;
}

// 將螢幕 UV 轉換成 World Space 的 View Direction
float3 GetWorldViewDirection(float2 uv, row_major float4x4 inverseProjection, row_major float4x4 inverseView)
{
    // 將 UV 的 [0, 1] 範圍轉換成 NDC 的 [-1, 1]
    float2 ndc = uv * 2.0f - 1.0f;
    
    // 因為 Texture UV 的 Y 軸向下，因此反轉成 NDC 的 Y 軸方向
    ndc.y = -ndc.y;
    
    // 建立位於遠裁切平面的 Clip Space 位置
    float4 clipPosition = float4(ndc.x, ndc.y, 1.0f, 1.0f);
    
    // 使用 inverse Projection 將 Clip Space 位置轉回 View Space
    float4 viewPosition = mul(clipPosition, inverseProjection);
    
    // 執行透視除法取得正確的 View Space 位置
    viewPosition /= viewPosition.w;
    
    // 從 Camera 原點指向 View Space 位置並正規化
    float3 viewDirection = normalize(viewPosition.xyz);
    
    // 使用 inverse View 將方向從 View Space 轉換到 World Space
    float3 worldDirection =
        mul(float4(viewDirection, 0.0f), inverseView).xyz;
    
    // 回傳正規化後的 World Space View Direction
    return normalize(worldDirection);
}

#endif
