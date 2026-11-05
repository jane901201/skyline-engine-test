// 將所有色塊的 RGBA 顏色綁定到 t0，以色塊編號索引
StructuredBuffer<float4> blockColors : register(t0);

// 定義 Vertex Shader 的輸入資料，只需要位置
struct VSInput
{
    // 接收 Mesh 頂點的位置（NDC 座標）
    float3 position : POSITION;
};

// 定義 Vertex Shader 傳給 Pixel Shader 的資料
struct VSOutput
{
    // 儲存裁切空間位置
    float4 position : SV_Position;
};

// 執行每個頂點的 Vertex Shader
VSOutput VSMain(VSInput input)
{
    // 建立 Vertex Shader 輸出資料
    VSOutput output;

    // Mesh 已經是 NDC 座標，直接輸出
    output.position = float4(input.position.xy, 0.0f, 1.0f);

    // 回傳 Vertex Shader 輸出
    return output;
}

// 執行每個 Pixel 的 Pixel Shader
float4 PSMain(float4 position : SV_Position, uint primitiveId : SV_PrimitiveID) : SV_Target0
{
    // 每個色塊由兩個連續的三角形組成，因此 SV_PrimitiveID / 2 就是色塊編號
    return blockColors[primitiveId / 2];
}
