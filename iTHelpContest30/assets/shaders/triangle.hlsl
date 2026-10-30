// 定義 Vertex Shader 接收的輸入資料
struct VSInput
{
    float4 position : POSITION; // 接收 Vertex Buffer 傳入的頂點位置
};

// 定義 Vertex Shader 傳出的輸出資料
struct VSOutput
{
    float4 position : SV_POSITION; // 輸出頂點經過 Vertex Shader 處理後的位置
};

// 接收頂點資料並輸出處理後的位置
VSOutput VSMain(VSInput input)
{
    // 建立 VSOutput 並將所有欄位初始化為 0
    VSOutput vsOut = (VSOutput) 0;

    // 將輸入的頂點位置直接傳給輸出位置
    vsOut.position = input.position;

    // 回傳處理完成的 Vertex Shader 輸出資料
    return vsOut;
}

// 接收 Vertex Shader 輸出並決定 Pixel 的最終顏色
float4 PSMain(VSOutput vsOut) : SV_Target0
{
    // 輸出不透明的紅色
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}