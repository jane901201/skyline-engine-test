// 定義圓周率常數供光照公式使用
static const float PI = 3.14159265359f;

cbuffer PbrConstants : register(b0)
{
    // 物件從 Local Space 轉換到 World Space 的矩陣
    row_major float4x4 world;

    // 物件從 Local Space 轉換到 Clip Space 的 World View Projection 矩陣
    row_major float4x4 worldViewProjection;

    // 相機在 World Space 中的位置
    float3 cameraPosition;

    // 用來符合 Constant Buffer 記憶體對齊需求的填充值
    float padding0;

    // 材質的基礎顏色
    float3 baseColor;

    // 材質的金屬度
    float metallic;

    // 太陽光在 World Space 中的方向
    float3 sunDirection;

    // 材質的粗糙度
    float roughness;

    // 太陽光顏色
    float3 sunColor;

    // 太陽光強度
    float sunIntensity;
};

struct VSInput
{
    // 頂點在 Local Space 中的位置
    float3 position : POSITION;

    // 頂點在 Local Space 中的法線
    float3 normal : NORMAL;

    // 頂點的 UV 座標
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    // 頂點在 Clip Space 中的位置
    float4 position : SV_POSITION;

    // 頂點在 World Space 中的位置
    float3 worldPosition : POSITION0;

    // 頂點在 World Space 中的法線
    float3 worldNormal : NORMAL;
};

// Vertex Shader 主函式
VSOutput VSMain(VSInput input)
{
    // 建立 Vertex Shader 的輸出資料
    VSOutput output;

    // 將頂點位置從 Local Space 轉換到 World Space
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);

    // 將頂點位置轉換到 Clip Space
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);

    // 儲存 World Space 頂點位置供 Pixel Shader 計算光照
    output.worldPosition = worldPosition.xyz;

    // 將法線轉換到 World Space 並正規化
    output.worldNormal = normalize(mul(input.normal, (float3x3) world));

    // 回傳轉換完成的頂點資料
    return output;
}

// 計算 GGX Normal Distribution Function
float DistributionGGX(float3 normal, float3 halfVector, float alpha)
{
    // 計算 alpha 的平方
    const float alphaSquared = alpha * alpha;

    // 計算法線與 Half Vector 的內積並限制在 0 到 1
    const float normalDotHalf = saturate(dot(normal, halfVector));

    // 計算 GGX Distribution 的分母中間項
    const float denominator = normalDotHalf * normalDotHalf * (alphaSquared - 1.0f) + 1.0f;

    // 回傳 GGX Normal Distribution 並避免分母過小
    return alphaSquared / max(PI * denominator * denominator, 0.0001f);
}

// 使用 Schlick-GGX 近似計算單一方向的 Geometry Term
float GeometrySchlickGGX(float normalDotDirection, float roughnessValue)
{
    // 根據粗糙度計算 Schlick-GGX 使用的 k 參數
    const float k = ((roughnessValue + 1.0f) * (roughnessValue + 1.0f)) / 8.0f;

    // 回傳 Geometry Term 並避免分母過小
    return normalDotDirection / max(normalDotDirection * (1.0f - k) + k, 0.0001f);
}

// 使用 Schlick Approximation 計算 Fresnel 反射率
float3 FresnelSchlick(float cosine, float3 reflectance)
{
    // 根據入射角度從基礎反射率逼近 Fresnel Effect
    return reflectance + (1.0f - reflectance) * pow(1.0f - saturate(cosine), 5.0f);
}



// Pixel Shader 主函式
float4 PSMain(VSOutput input) : SV_TARGET
{
    // 正規化插值後的 World Space 法線
    const float3 normal = normalize(input.worldNormal);

    // 計算表面指向相機的 View Direction
    const float3 viewDirection = normalize(cameraPosition - input.worldPosition);

    // 正規化光源方向
    const float3 lightDirection = normalize(sunDirection);

    // 計算 View Direction 與 Light Direction 中間的 Half Vector
    const float3 halfVector = normalize(viewDirection + lightDirection);

    // 計算法線與光源方向的內積
    const float normalDotLight = saturate(dot(normal, lightDirection));

    // 計算法線與視線方向的內積
    const float normalDotView = saturate(dot(normal, viewDirection));

    // 根據金屬度在非金屬預設反射率與 Base Color 之間插值
    const float3 f0 = lerp(0.04f.xxx, baseColor, metallic);

    // 將 Roughness 平方轉換為 GGX 使用的 alpha 並設定最小值
    const float alpha = max(roughness * roughness, 0.0025f);

    // 計算 GGX Normal Distribution
    const float distribution = DistributionGGX(normal, halfVector, alpha);

    // 分別計算 View 與 Light 方向的 Geometry Term 後相乘
    const float geometry = GeometrySchlickGGX(normalDotView, roughness) *
                           GeometrySchlickGGX(normalDotLight, roughness);

    // 計算目前角度下的 Fresnel 反射率
    const float3 fresnel = FresnelSchlick(dot(halfVector, viewDirection), f0);

    // 使用 Cook-Torrance BRDF 計算 Specular Reflection
    const float3 specular = distribution * geometry * fresnel /
                            max(4.0f * normalDotView * normalDotLight, 0.0001f);

    // 計算 Diffuse Reflection 所占比例並排除金屬材質的 Diffuse 成分
    const float3 diffuseWeight = (1.0f - fresnel) * (1.0f - metallic);

    // 根據光源顏色與強度計算入射 Radiance
    const float3 radiance = sunColor * sunIntensity;

    // 結合 Lambert Diffuse 與 Cook-Torrance Specular 計算直接光照
    const float3 directLighting = (diffuseWeight * baseColor / PI + specular) * radiance * normalDotLight;

    // 加入簡單的環境光並降低金屬材質的環境 Diffuse
    const float3 ambient = baseColor * (0.025f * (1.0f - metallic));

    // 合併直接光照與環境光得到 HDR 顏色
    const float3 mappedColor = directLighting + ambient;

    // 使用 Reinhard Tone Mapping 將 HDR 顏色映射到較低動態範圍
    const float3 toneMapped = mappedColor / (mappedColor + 1.0f);

    // 執行 Gamma Correction 後輸出不透明顏色
    return float4(pow(toneMapped, 1.0f / 2.2f), 1.0f);
}