// 場景 Pass 輸出的顏色
Texture2D<float4> sceneColor : register(t0);

// ID 輔助 Pass 輸出的編碼色塊編號
Texture2D<float4> blockIds : register(t1);

// 後處理使用的常數
cbuffer PostConstants : register(b0)
{
    // 目前選取的色塊 key（色塊編號 + 1），0 表示沒有選取
    uint selectedKey;

    // 對齊到 16 bytes
    uint3 reserved;

    // 選取色塊的疊色（rgb 為顏色、a 為混合強度）
    float4 highlightColor;

    // 選取色塊的外框顏色
    float4 outlineColor;
};

// 外框寬度（像素）
static const int OutlineWidth = 3;

// 未選取色塊被壓暗的倍率
static const float DimFactor = 0.35f;

// 以螢幕空間的全螢幕三角形取代 Vertex Buffer，只需要 SV_VertexID
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    // 產生 (0,0) (2,0) (0,2) 三個 UV，三角形會覆蓋整個螢幕
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);

    // 將 UV 轉換成 NDC，並翻轉 Y 軸
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}

// 讀取指定像素的色塊 key；Load 不做過濾，才能保留精確的 ID
uint loadKey(int2 pixel)
{
    // 讀取 ID 紋理並還原成 0～255 的整數 byte
    uint3 bytes = (uint3)round(blockIds.Load(int3(pixel, 0)).rgb * 255.0f);

    // 把三個 byte 組回 key
    return bytes.x | (bytes.y << 8) | (bytes.z << 16);
}

// 執行每個 Pixel 的後處理
float4 PSMain(float4 position : SV_Position) : SV_Target0
{
    // 取得目前像素座標
    int2 pixel = int2(position.xy);

    // 讀取場景顏色
    float4 color = sceneColor.Load(int3(pixel, 0));

    // 沒有選取任何色塊時直接輸出場景顏色
    if (selectedKey == 0)
        return color;

    // 其他色塊壓暗，讓選取的色塊更突出
    if (loadKey(pixel) != selectedKey)
    {
        color.rgb *= DimFactor;
        return color;
    }

    // 選取的色塊先疊上高亮顏色
    color.rgb = lerp(color.rgb, highlightColor.rgb, highlightColor.a);

    // 上下左右任一方向在外框寬度內碰到其他色塊，就視為外框
    if (loadKey(pixel + int2(OutlineWidth, 0)) != selectedKey || loadKey(pixel - int2(OutlineWidth, 0)) != selectedKey ||
        loadKey(pixel + int2(0, OutlineWidth)) != selectedKey || loadKey(pixel - int2(0, OutlineWidth)) != selectedKey)
        color.rgb = outlineColor.rgb;

    // 輸出最終顏色
    return color;
}
