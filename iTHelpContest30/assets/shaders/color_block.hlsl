// 定義單一色塊在 Structured Buffer 中的資料格式
struct ColorBlockData
{
    // 儲存色塊中心位置
    float2 center;

    // 儲存色塊寬高的一半
    float2 halfSize;

    // 儲存色塊的 RGBA 顏色
    float4 color;
};

// 將色塊資料綁定到 t0 Shader Resource slot
StructuredBuffer<ColorBlockData> colorBlocks : register(t0);

// 定義 Vertex Shader 的輸入資料
struct VSInput
{
    // 接收 Vertex Buffer 傳入的頂點位置
    float4 position : POSITION;

    // 接收目前 instance 的 index
    uint instanceId : SV_InstanceID;
};

// 定義 Vertex Shader 傳給 Pixel Shader 的資料
struct VSOutput
{
    // 儲存轉換後的裁切空間位置
    float4 position : SV_Position;

    // 傳遞目前色塊的顏色
    float4 color : COLOR0;
};

// 執行每個頂點的 Vertex Shader
VSOutput VSMain(VSInput input)
{
    // 根據目前 instance index 取得對應的色塊資料
    ColorBlockData block = colorBlocks[input.instanceId];

    // 建立 Vertex Shader 輸出資料
    VSOutput output;

    // 將共用四邊形頂點縮放至色塊大小，再平移至色塊中心
    output.position = float4(block.center + input.position.xy * block.halfSize, 0.0f, 1.0f);

    // 將目前色塊顏色傳給 Pixel Shader
    output.color = block.color;

    // 回傳 Vertex Shader 輸出
    return output;
}

// 執行每個 Pixel 的 Pixel Shader
float4 PSMain(VSOutput input) : SV_Target0
{
    // 將 Vertex Shader 傳入的顏色直接輸出到 Render Target
    return input.color;
}