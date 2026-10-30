cbuffer ShaderConstants : register(b0)
{
    // 螢幕解析度
    float2 u_resolution;
};

// 根據輸入座標產生 0 到 1 之間的偽隨機值
float Random(float2 st)
{
    // 將座標轉換成雜湊結果並只保留小數部分
    return frac(sin(dot(st, float2(12.989f, 78.233f)) * 43758.5453123f));
}

// 根據二維座標計算 Value Noise
float Noise(float2 st)
{
    // 取得目前所在的 grid 座標
    float2 i = floor(st);

    // 取得目前位置在 grid 內的局部座標
    float2 f = frac(st);

    // 取得左下角的隨機值
    float a = Random(i);

    // 取得右下角的隨機值
    float b = Random(i + float2(1.0f, 0.0f));

    // 取得左上角的隨機值
    float c = Random(i + float2(0.0f, 1.0f));

    // 取得右上角的隨機值
    float d = Random(i + float2(1.0f, 1.0f));

    // 使用 Smoothstep 曲線平滑局部座標
    float2 u = f * f * (3.0f - 2.0f * f);

    // 對四個角落的隨機值進行雙線性插值
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0f - u.x)
    + (d - b) * u.x * u.y;
}

#define OCTAVES 6

// 疊加多層不同頻率與振幅的 Noise 產生 Fractal Brownian Motion
float Fbm(float2 st)
{
    // 儲存累積的 Noise 值
    float value = 0.0f;

    // 設定第一層 Noise 的振幅
    float amplitude = 0.5f;

    // 要求 Compiler 展開迴圈以減少執行時的迴圈控制成本
    [unroll]
    for (int i = 0; i < OCTAVES; ++i)
    {
        // 將目前頻率的 Noise 乘上振幅後累加
        value += amplitude * Noise(st);

        // 將座標放大兩倍以提高下一層 Noise 的頻率
        st *= 2.0f;

        // 將下一層 Noise 的振幅降低一半
        amplitude *= 0.5f;
    }

    // 回傳所有 octave 疊加後的結果
    return value;
}

// 使用 Vertex ID 產生 Fullscreen Triangle 的頂點位置
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    // 根據 Vertex ID 產生三角形頂點座標
    float2 position = float2((vertexId << 1) & 2, vertexId & 2);

    // 將座標轉換到 NDC 範圍並翻轉 Y 軸
    return float4(position * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}

// 根據 Pixel 的螢幕座標計算最終顏色
float4 PSMain(float4 position : SV_Position) : SV_Target
{
    // 將 Pixel 座標轉換成 0 到 1 的 UV 座標
    float2 st = position.xy / u_resolution.xy;

    // 根據螢幕 Aspect Ratio 修正 X 軸比例避免 Noise 被拉伸
    st.x *= u_resolution.x / u_resolution.y;

    // 放大取樣座標並計算 FBM Noise
    float value = Fbm(st * 3.0f);

    // 將 Noise 值輸出成灰階顏色
    return float4(value, value, value, 1.0f);
}