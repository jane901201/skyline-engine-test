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

// 執行每個頂點的 Vertex Shader（與場景 Pass 相同，確保兩個 Pass 的幾何完全一致）
VSOutput VSMain(VSInput input)
{
    // 建立 Vertex Shader 輸出資料
    VSOutput output;

    // Mesh 已經是 NDC 座標，直接輸出
    output.position = float4(input.position.xy, 0.0f, 1.0f);

    // 回傳 Vertex Shader 輸出
    return output;
}

// 將色塊編號編碼成顏色寫入 ID 輔助 Pass 的 Render Target
float4 PSMain(float4 position : SV_Position, uint primitiveId : SV_PrimitiveID) : SV_Target0
{
    // 色塊編號 + 1 作為 key，0 保留給「沒有任何色塊」的背景
    uint key = primitiveId / 2 + 1;

    // 以 R、G、B 三個 8-bit 通道各存 key 的一個 byte，最多可編碼約 1600 萬個色塊
    return float4(float(key & 0xFF) / 255.0f, float((key >> 8) & 0xFF) / 255.0f, float((key >> 16) & 0xFF) / 255.0f,
                  1.0f);
}
