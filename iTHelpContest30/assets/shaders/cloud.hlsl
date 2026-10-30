#include "common.hlsl"

// 定義單一雲格所需的資料結構
struct CloudData
{
    // 儲存雲格在世界空間中的位置
    float3 position;

    // 儲存雲格的密度
    float density;
};

// 宣告 Compute Shader 可寫入的雲格資料 Buffer 並綁定到 u0
RWStructuredBuffer<CloudData> cloudDataOutput : register(u0);

// 宣告唯讀的雲格資料 Buffer 並綁定到 t0
StructuredBuffer<CloudData> cloudData : register(t0);

// 宣告雲層相關常數 Buffer 並綁定到 b1
cbuffer CloudConstants : register(b1)
{
    // 雲格總數
    uint cloudCount;

    // 雲層底部的世界高度
    float cloudBottomHeight;

    // 雲層頂部的世界高度
    float cloudTopHeight;

    // 單一雲格的尺寸
    float cloudCellSize;

    // 目前累積時間
    float time;

    // 控制雜訊取樣位置的縮放倍率
    float densityScale;

    // 控制雲層移動速度
    float cloudSpeed;

    // 控制光線穿過雲層時的消光強度
    float extinction;

    // 主視線 Ray March 的基礎取樣步數
    uint rayMarchSteps;

    // 雲層本身的顏色
    float3 cloudColor;

    // 控制天空被雲層覆蓋的比例
    float coverage;

    // 控制雲密度的整體倍率
    float densityMultiplier;

    // 控制大型雲形雜訊的取樣尺度
    float baseNoiseScale;

    // 控制細節雜訊的取樣尺度
    float detailNoiseScale;

    // 控制細節雜訊對雲層邊緣的侵蝕程度
    float detailErosion;

    // 控制雲底淡入距離
    float bottomFadeDistance;

    // 控制雲頂淡出距離
    float topFadeDistance;

    // 控制雲層對太陽光的吸收程度
    float lightAbsorption;

    // 控制 FBM 疊加的 Noise octave 數量
    uint noiseIterations;

    // 控制朝光源方向進行 Ray March 的步數
    uint lightMarchSteps;

    // 控制光線 Ray March 每一步的世界空間距離
    float lightStepSize;

    // 用於 Constant Buffer 記憶體對齊
    float _cloudPadding;

    // 太陽光照射方向
    float3 sunDirection;

    // 太陽光強度
    float sunIntensity;

    // 太陽光顏色
    float3 sunColor;

    // 用於 Constant Buffer 記憶體對齊
    float _sunPadding;
};

// 產生以三維座標為輸入的偽隨機 Hash 值
float Hash(float3 position)
{
    // 將輸入縮放偏移後限制到 0 到 1 的週期範圍
    position = frac(position * 0.3183099f + 0.1f);

    // 放大座標以增加不同位置之間的數值變化
    position *= 17.0f;

    // 混合三個座標分量並取小數部分得到偽隨機值
    return frac(position.x * position.y * position.z * (position.x + position.y + position.z));
}

// 設定每個 Compute Shader Thread Group 包含 64 個 X 軸執行緒
[numthreads(64, 1, 1)]

// Compute Shader 主程式
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // 取得目前執行緒所對應的雲格索引
    const uint index = dispatchThreadId.x;

    // 超出雲格數量時停止處理
    if (index >= cloudCount)
        return;

    // 由索引直接建立穩定的雲格位置，避免每幀累加造成漂移誤差
    const uint gridWidth = 8;

    // 設定雲格陣列在深度方向的格數
    const uint gridDepth = 8;

    // 計算單一高度層包含的雲格數量
    const uint layerSize = gridWidth * gridDepth;

    // 計算目前雲格位於第幾個高度層
    const uint layer = index / layerSize;

    // 取得目前高度層內的局部索引
    const uint cellIndex = index % layerSize;

    // 從局部索引取得 X 軸格子座標
    const uint x = cellIndex % gridWidth;

    // 從局部索引取得 Y 軸格子座標
    const uint y = cellIndex / gridWidth;

    // 讓相鄰雲格互相重疊，避免球狀密度之間出現明顯空洞
    const float horizontalSpacing = 6.0f;

    // 計算整個雲場在移動方向上的循環長度
    const float cloudFieldDepth = horizontalSpacing * gridDepth;

    // 根據時間與速度計算循環移動後的 Y 軸位置
    const float animatedY = fmod(float(y) * horizontalSpacing + cloudSpeed * time, cloudFieldDepth);

    // 建立目前雲格的資料
    CloudData cloud;

    // 根據格子索引與高度層計算雲格的世界空間位置
    cloud.position = float3(
        (float(x) - (float(gridWidth) - 1.0f) * 0.5f) * horizontalSpacing,
        animatedY + 4.0f,
        lerp(cloudBottomHeight + 0.8f, cloudTopHeight - 0.8f, float(layer)));

    // 根據雲格位置與時間產生動態密度雜訊
    const float noise = Hash(cloud.position * densityScale + float3(0.0f, time * 0.03f, 0.0f));

    // 高度淡化距離應由雲層厚度決定，不應受雲格半徑影響
    const float heightFadeDistance = max((cloudTopHeight - cloudBottomHeight) * 0.25f, 0.001f);

    // 計算接近雲底時的密度淡入比例
    const float bottomFade = saturate((cloud.position.z - cloudBottomHeight) / heightFadeDistance);

    // 計算接近雲頂時的密度淡出比例
    const float topFade = saturate((cloudTopHeight - cloud.position.z) / heightFadeDistance);

    // 將隨機雜訊映射成基礎雲密度
    const float baseDensity = lerp(0.45f, 1.0f, noise);

    // 將基礎密度乘上雲底與雲頂的高度淡化
    cloud.density = baseDensity * bottomFade * topFade;

    // 將計算完成的雲格資料寫入輸出 Buffer
    cloudDataOutput[index] = cloud;
}

// Fullscreen Triangle 的 Vertex Shader
FullscreenVSOutput VSMain(uint vertexId : SV_VertexID)
{
    // 根據 Vertex ID 產生 Fullscreen Triangle 頂點資料
    return GetFullscreenTriangleVertex(vertexId);
}

// 計算三維 Value Noise
float ValueNoise(float3 position)
{
    // 取得目前取樣位置所在的整數網格座標
    const float3 cell = floor(position);

    // 取得網格內部的局部小數座標
    float3 interpolation = frac(position);

    // 使用 Smooth Hermite 曲線平滑網格內的插值比例
    interpolation = interpolation * interpolation * (3.0f - 2.0f * interpolation);

    // 取得網格角點 000 的偽隨機值
    const float n000 = Hash(cell + float3(0.0f, 0.0f, 0.0f));

    // 取得網格角點 100 的偽隨機值
    const float n100 = Hash(cell + float3(1.0f, 0.0f, 0.0f));

    // 取得網格角點 010 的偽隨機值
    const float n010 = Hash(cell + float3(0.0f, 1.0f, 0.0f));

    // 取得網格角點 110 的偽隨機值
    const float n110 = Hash(cell + float3(1.0f, 1.0f, 0.0f));

    // 取得網格角點 001 的偽隨機值
    const float n001 = Hash(cell + float3(0.0f, 0.0f, 1.0f));

    // 取得網格角點 101 的偽隨機值
    const float n101 = Hash(cell + float3(1.0f, 0.0f, 1.0f));

    // 取得網格角點 011 的偽隨機值
    const float n011 = Hash(cell + float3(0.0f, 1.0f, 1.0f));

    // 取得網格角點 111 的偽隨機值
    const float n111 = Hash(cell + float3(1.0f, 1.0f, 1.0f));

    // 沿 X 軸插值第一組角點
    const float nx00 = lerp(n000, n100, interpolation.x);

    // 沿 X 軸插值第二組角點
    const float nx10 = lerp(n010, n110, interpolation.x);

    // 沿 X 軸插值第三組角點
    const float nx01 = lerp(n001, n101, interpolation.x);

    // 沿 X 軸插值第四組角點
    const float nx11 = lerp(n011, n111, interpolation.x);

    // 沿 Y 軸插值 Z 為 0 的兩組結果
    const float nxy0 = lerp(nx00, nx10, interpolation.y);

    // 沿 Y 軸插值 Z 為 1 的兩組結果
    const float nxy1 = lerp(nx01, nx11, interpolation.y);

    // 沿 Z 軸完成最後插值得到三維 Value Noise
    return lerp(nxy0, nxy1, interpolation.z);
}

// 使用多層 Value Noise 計算 FBM
float Fbm(float3 position)
{
    // 儲存所有 octave 疊加後的 Noise
    float value = 0.0f;

    // 設定第一層 Noise 的振幅
    float amplitude = 0.5f;

    // 儲存所有振幅總和供最後正規化
    float amplitudeSum = 0.0f;

    // 強制保留動態迴圈避免編譯器完全展開
    [loop]

    // 根據指定 octave 數量疊加多層 Noise
    for (uint iteration = 0; iteration < noiseIterations; ++iteration)
    {
        // 將目前頻率的 Noise 乘上振幅後累加
        value += ValueNoise(position) * amplitude;

        // 累加目前 octave 的振幅
        amplitudeSum += amplitude;

        // 提高下一層 Noise 頻率並加入偏移避免規律重疊
        position = position * 2.03f + float3(17.1f, 31.7f, 11.9f);

        // 每個 octave 將振幅減半
        amplitude *= 0.5f;
    }

    // 除以總振幅讓結果維持穩定範圍
    return value / amplitudeSum;
}

// 將數值從一個範圍重新映射到另一個範圍
float Remap(float value, float oldMinimum, float oldMaximum, float newMinimum, float newMaximum)
{
    // 將輸入值正規化到 0 到 1 並避免除以零
    const float normalizedValue = saturate(
        (value - oldMinimum) / max(oldMaximum - oldMinimum, 0.0001f));

    // 將正規化結果插值到新的數值範圍
    return lerp(newMinimum, newMaximum, normalizedValue);
}

// 根據高度與形狀雜訊產生垂直方向的雲層輪廓
float CloudHeightProfile(float heightFraction, float shapeNoise)
{
    // 雲底保持較平坦，雲頂則依低頻雜訊形成向上隆起的輪廓
    const float bottom = smoothstep(0.0f, 0.12f, heightFraction);

    // 根據形狀雜訊改變雲頂開始淡出的高度
    const float topStart = lerp(0.58f, 0.82f, shapeNoise);

    // 計算雲頂的淡出輪廓
    const float top = 1.0f - smoothstep(topStart, 1.0f, heightFraction);

    // 結合雲底與雲頂輪廓
    return bottom * top;
}

// 取樣指定世界座標的體積雲密度
float SampleCloudDensity(float3 worldPosition)
{
    // 位於雲層高度範圍外時不產生密度
    if (worldPosition.z <= cloudBottomHeight || worldPosition.z >= cloudTopHeight)
        return 0.0f;

    // 計算整個雲層的厚度
    const float cloudThickness = cloudTopHeight - cloudBottomHeight;

    // 將目前高度轉換成雲層內 0 到 1 的高度比例
    const float heightFraction = saturate((worldPosition.z - cloudBottomHeight) / cloudThickness);

    // 根據時間與速度產生水平方向的風位移
    const float3 windOffset = float3(cloudSpeed * time * 0.35f, cloudSpeed * time, 0.0f);

    // 低頻天氣圖將天空分成晴朗區與成片的雲層
    const float weather = Fbm(float3(
        (worldPosition.xy + windOffset.xy) * baseNoiseScale * 0.5f,
        time * 0.001f));

    // 扭曲取樣座標，避免雲朵看起來像規則堆疊的雜訊球
    const float3 warpPosition = (worldPosition + windOffset) * baseNoiseScale;

    // 分別從三個偏移位置取樣 Noise 形成三維 Domain Warp
    const float3 domainWarp = float3(
        ValueNoise(warpPosition + 13.7f),
        ValueNoise(warpPosition + 37.1f),
        ValueNoise(warpPosition + 71.9f)) - 0.5f;

    // 使用 Domain Warp 後的位置取得大型雲形
    const float baseShape = Fbm(warpPosition + domainWarp * 1.8f);

    // 根據全域 coverage 與天氣圖計算局部覆蓋程度
    const float localCoverage = saturate(coverage + (weather - 0.5f) * 0.9f);

    // 以高度場決定雲頂，再用三維雜訊打散表面，形成相連的隆起雲團
    const float billow = ValueNoise(float3(warpPosition.xy * 0.55f, 2.3f));

    // 結合隆起 Noise 與基礎形狀計算局部雲頂高度
    const float surfaceHeight = 0.32f + billow * 0.46f + (baseShape - 0.5f) * 0.4f;

    // 根據天氣圖與局部覆蓋程度建立雲層遮罩
    const float coverageMask = smoothstep(0.65f - localCoverage, 0.75f - localCoverage, weather);

    // 根據表面高度與目前高度差計算初始體積密度
    float density = saturate((surfaceHeight - heightFraction) * densityMultiplier) * coverageMask;

    // 高頻雜訊只侵蝕雲層邊緣，保留內部厚實的體積
    const float detail = Fbm((worldPosition + windOffset * 1.7f) * detailNoiseScale);

    // 密度越低的邊緣區域受到越強的細節侵蝕
    const float edgeErosion = detail * detailErosion * (1.0f - density);

    // 從原始密度扣除細節侵蝕並限制在有效範圍
    density = saturate(density - edgeErosion);

    // 計算雲底的平滑淡入
    const float bottomFade = smoothstep(
        cloudBottomHeight, cloudBottomHeight + bottomFadeDistance, worldPosition.z);

    // 計算雲頂的平滑淡出
    const float topFade = 1.0f - smoothstep(
        cloudTopHeight - topFadeDistance, cloudTopHeight, worldPosition.z);

    // 套用上下邊界淡化後回傳最終密度
    return density * bottomFade * topFade;
}

// 計算指定位置朝太陽方向的光線穿透率
float SampleLightTransmittance(float3 worldPosition)
{
    // 累積光線沿途穿過的 Optical Depth
    float opticalDepth = 0.0f;

    // 從目前取樣位置開始沿太陽方向移動
    float3 lightSamplePosition = worldPosition;

    // 保留動態光線取樣迴圈
    [loop]

    // 沿太陽方向執行多次 Ray March
    for (uint step = 0; step < lightMarchSteps; ++step)
    {
        // 將光線取樣位置往太陽方向前進
        lightSamplePosition += sunDirection * lightStepSize;

        // 將目前位置的雲密度乘上步長後累積成 Optical Depth
        opticalDepth += SampleCloudDensity(lightSamplePosition) * lightStepSize;
    }

    // 使用 Beer-Lambert Law 將 Optical Depth 轉換成光線穿透率
    return exp(-opticalDepth * lightAbsorption);
}

// 計算 Henyey-Greenstein Phase Function
float HenyeyGreenstein(float cosineAngle, float anisotropy)
{
    // 預先計算非等向性參數的平方
    const float anisotropySquared = anisotropy * anisotropy;

    // 計算 Phase Function 分母並避免數值過小
    const float denominator = max(
        1.0f + anisotropySquared - 2.0f * anisotropy * cosineAngle,
        0.001f);

    // 除以 4 PI，避免朝向太陽時的散射能量直接超出 SDR 色彩範圍
    return (1.0f - anisotropySquared) / (12.5663706f * pow(denominator, 1.5f));
}

// Fullscreen Pass 的 Pixel Shader
float4 PSMain(FullscreenVSOutput input) : SV_TARGET
{
    // 根據螢幕 UV 與反投影矩陣取得世界空間視線方向
    const float3 rayDirection = GetWorldViewDirection(input.uv, inverseProjection, inverseView);

    // 使用攝影機世界座標作為 Ray Origin
    const float3 rayOrigin = cameraPosition;

    // 視線幾乎平行雲層平面時直接略過避免除以零
    if (abs(rayDirection.z) <= 0.0001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 計算視線與雲層底部平面的交點距離
    const float distanceToBottom = (cloudBottomHeight - rayOrigin.z) / rayDirection.z;

    // 計算視線與雲層頂部平面的交點距離
    const float distanceToTop = (cloudTopHeight - rayOrigin.z) / rayDirection.z;

    // 取得最靠近攝影機且位於前方的雲層進入距離
    const float rayEntryDistance = max(min(distanceToBottom, distanceToTop), 0.0f);

    // 限制接近地平線時的取樣距離，避免固定步數被拉得過疏而產生水平條紋
    const float maximumCloudDistance = 360.0f;

    // 取得雲層離開距離並限制最大可見距離
    const float rayExitDistance = min(max(distanceToBottom, distanceToTop), maximumCloudDistance);

    // 沒有有效雲層區間時直接輸出透明
    if (rayEntryDistance >= rayExitDistance)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 依照路徑長度增加步數，確保完整走完雲層且每步不超過一個世界單位
    const float intervalLength = rayExitDistance - rayEntryDistance;

    // 在設定的基礎步數與每 0.5 世界單位一次取樣之間選擇較高值
    const uint sampleCount = max(rayMarchSteps, (uint) ceil(intervalLength / 0.5f));

    // 計算每一次主視線 Ray March 前進的距離
    const float stepSize = intervalLength / float(sampleCount);

    // 使用固定的步驟中點取樣，避免在沒有時間抗鋸齒時出現閃爍顆粒
    float rayDistance = rayEntryDistance + stepSize * 0.5f;

    // 初始光線穿透率設為完全透明
    float transmittance = 1.0f;

    // 儲存沿視線累積的散射顏色
    float3 accumulatedColor = 0.0f;

    // 計算視線方向與太陽方向之間的夾角 Cosine
    const float viewToSun = clamp(dot(rayDirection, sunDirection), -1.0f, 1.0f);

    // 計算朝太陽方向觀看時的 Forward Scattering 強度
    const float forwardScattering = min(HenyeyGreenstein(viewToSun, 0.58f), 1.0f);

    // 沿攝影機視線穿過雲層進行 Ray March
    for (uint step = 0; step < sampleCount; ++step)
    {
        // 根據目前距離計算世界空間取樣位置
        const float3 samplePosition = rayOrigin + rayDirection * rayDistance;

        // 超過雲層離開位置時停止取樣
        if (rayDistance >= rayExitDistance)
            break;

        // 取得目前位置的雲密度
        const float density = SampleCloudDensity(samplePosition);

        // 只有存在足夠密度時才進行昂貴的光照計算
        if (density > 0.001f)
        {
            // 每個有效密度樣本都計算陰影，避免沿用不同位置的光線穿透率
            const float lightTransmittance = SampleLightTransmittance(samplePosition);

            // 方向密度差強化受光面的形狀，搭配體積陰影保留厚度
            const float lightFacing = saturate((density -
                SampleCloudDensity(samplePosition + sunDirection * 0.6f)) / 0.6f);

            // 計算目前取樣點在雲層中的垂直高度比例
            const float heightFraction = saturate(
                (samplePosition.z - cloudBottomHeight) / (cloudTopHeight - cloudBottomHeight));

            // 雲底保留淡藍灰陰影，越接近雲頂則獲得越明亮的環境光
            const float3 ambientColor = lerp(
                float3(0.32f, 0.29f, 0.43f),
                float3(0.62f, 0.56f, 0.68f),
                heightFraction);

            // 結合光線穿透率 Forward Scattering 與表面朝光程度計算直射光
            const float directLight = lightTransmittance * (0.45f + forwardScattering * 0.4f + lightFacing * 1.4f);

            // 將環境光與太陽直射光組合成最終光照
            const float3 lighting = ambientColor + sunColor * sunIntensity * directLight;

            // 使用 Beer-Lambert Law 將目前步驟的密度轉換成不透明度
            const float sampleOpacity = 1.0f - exp(-density * extinction * stepSize);

            // 根據剩餘穿透率累積目前樣本的散射顏色
            accumulatedColor += transmittance * sampleOpacity * cloudColor * lighting;

            // 扣除目前樣本吸收與散射掉的光線
            transmittance *= 1.0f - sampleOpacity;
        }

        // 光線幾乎完全被遮蔽時提前結束 Ray March
        if (transmittance <= 0.02f)
        {
            break;
        }

        // 將視線前進到下一個取樣位置
        rayDistance += stepSize;
    }

    // 將剩餘穿透率轉換成最終雲層不透明度
    const float opacity = saturate(1.0f - transmittance);

    // 將累積顏色除以不透明度取得未預乘的雲層顏色
    const float3 resolvedColor = opacity > 0.0001f ? accumulatedColor / opacity : cloudColor;

    // 柔和壓縮高光，保留暖色而不直接截斷成白色
    const float3 displayColor = 1.0f - exp(-resolvedColor * 1.5f);

    // 接近最大雲層距離時逐漸降低可見度避免突然截斷
    const float horizonFade = 1.0f - smoothstep(260.0f, 360.0f, rayEntryDistance);

    // 輸出 Tone Mapping 後的雲層顏色與套用地平線淡出的 Alpha
    return float4(displayColor, opacity * horizonFade);
}