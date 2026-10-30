cbuffer ShaderConstants : register(b0)
{
    // 滑鼠位置，用來控制相機視角
    float2 mousePos;

    // 經過時間，用來驅動場景動畫
    float time;

    // 畫面的寬高比
    float aspectRatio;
};

struct Light
{
    // 光源類型，0 為 Directional Light，1 為 Point Light
    int type;

    // Directional Light 的照射方向
    float3 dir;

    // Point Light 在 World Space 中的位置
    float3 center;

    // 光源強度
    float intensity;

    // 光源顏色
    float3 color;
};

struct PBRMat
{
    // 材質的 Base Color
    float3 color;

    // 材質的 Roughness
    float roughness;

    // 材質的 Metallic
    float metallic;

    // 材質的 Ambient Occlusion
    float ao;
};

struct HitSurface
{
    // Ray 與物體相交的 World Space 位置
    float3 hitPoint;

    // 相交表面的法線
    float3 normal;

    // 相交表面的 PBR 材質
    PBRMat material;
};

struct Ray
{
    // Ray 的起點
    float3 origin;

    // Ray 的方向
    float3 direction;
};

struct ISObj
{
    // Ray 或採樣點與物體之間的距離
    float distance;

    // 相交物件類型
    int type;

    // 相交物件 ID
    int id;
};

struct VSOutput
{
    // 頂點在 Clip Space 中的位置
    float4 position : SV_Position;

    // Fullscreen Triangle 的 UV 座標
    float2 uv : TEXCOORD0;
};

// 定義圓周率供 PBR 光照公式使用
static const float PI = 3.14159265359f;

// Ray Marching 判定命中的最小距離
static const float DIST_MIN = 0.001f;

// Ray Marching 允許前進的最大距離
static const float DIST_MAX = 100.0f;

// 表示沒有命中任何物件
static const int OBJECT_NONE = -1;

// 表示一般可著色表面
static const int OBJECT_SURFACE = 0;

// 建立並初始化 ISObj
ISObj MakeISObj(float distance, int type, int id)
{
    ISObj result;

    // 設定距離資訊
    result.distance = distance;

    // 設定物件類型
    result.type = type;

    // 設定物件 ID
    result.id = id;

    // 回傳建立完成的相交資訊
    return result;
}

// 建立並初始化 Ray
Ray MakeRay(float3 origin, float3 direction)
{
    Ray result;

    // 設定 Ray 起點
    result.origin = origin;

    // 將方向正規化後存入 Ray
    result.direction = normalize(direction);

    return result;
}

// 建立從指定位置朝向光源的 Ray
Ray LightRay(float3 origin, Light light)
{
    // Directional Light 直接使用固定光線方向
    if (light.type == 0)
        return MakeRay(origin, light.dir);

    // Point Light 使用目前位置指向光源位置的方向
    return MakeRay(origin, light.center - origin);
}

// 取得目前位置到光源的有效距離
float LightDist(float3 origin, Light light)
{
    // Directional Light 視為位於無限遠
    if (light.type == 0)
        return DIST_MAX;

    // 計算目前位置到 Point Light 的距離
    return length(light.center - origin);
}

// 計算指定位置到球體表面的 Signed Distance
float SphereSDF(float3 position, float3 center, float radius)
{
    return length(position - center) - radius;
}

// 計算指定位置到 Box 表面的 Signed Distance
float BoxSDF(float3 position, float3 center, float3 halfExtent)
{
    // 計算位置相對於 Box 各軸邊界的距離
    const float3 offset = abs(position - center) - halfExtent;

    // 結合 Box 外部與內部距離得到完整 SDF
    return length(max(offset, 0.0f)) + min(max(offset.x, max(offset.y, offset.z)), 0.0f);
}

// 將位置繞 Y 軸旋轉
float3 RotateAroundY(float3 position, float angle)
{
    // 計算旋轉角度的正弦值
    const float sine = sin(angle);

    // 計算旋轉角度的餘弦值
    const float cosine = cos(angle);

    // 套用 Y 軸 Rotation Matrix
    return float3(position.x * cosine - position.z * sine,
                  position.y,
                  position.x * sine + position.z * cosine);
}

// 計算場景中距離指定位置最近的物體
ISObj SceneSDF(float3 position)
{
    // 先將 y = -1 的無限平面設為目前最近物體
    ISObj nearestObject = MakeISObj(position.y + 1.0f, OBJECT_SURFACE, 0);

    // 計算位置到左側球體的距離
    const float sphereDistance = SphereSDF(position, float3(-1.35f, 0.0f, 4.0f), 1.0f);

    // 若球體更接近則更新最近物體
    if (sphereDistance < nearestObject.distance)
        nearestObject = MakeISObj(sphereDistance, OBJECT_SURFACE, 1);

    // 將位置轉換到旋轉後 Box 的 Local Space
    const float3 boxPosition = RotateAroundY(position - float3(1.35f, 0.0f, 4.0f), 0.45f);

    // 計算位置到 Box 的 Signed Distance
    const float boxDistance = BoxSDF(boxPosition, 0.0f.xxx, float3(0.85f, 1.0f, 0.85f));

    // 若 Box 更接近則更新最近物體
    if (boxDistance < nearestObject.distance)
        nearestObject = MakeISObj(boxDistance, OBJECT_SURFACE, 2);

    // 計算具有上下浮動動畫的小球 Signed Distance
    const float movingSphereDistance =
        SphereSDF(position, float3(0.0f, -0.35f + sin(time * 1.3f) * 0.12f, 2.25f), 0.55f);

    // 若移動球體更接近則更新最近物體
    if (movingSphereDistance < nearestObject.distance)
        nearestObject = MakeISObj(movingSphereDistance, OBJECT_SURFACE, 3);

    // 回傳目前位置最近的場景物件
    return nearestObject;
}

// 根據物件 ID 取得對應的 PBR 材質
PBRMat GetMaterial(int id, float3 position)
{
    PBRMat material;

    // 預設不降低 Ambient Occlusion
    material.ao = 1.0f;

    // 設定地板材質
    if (id == 0)
    {
        // 根據 XZ 座標產生棋盤格索引
        const float checker = fmod(floor(position.x) + floor(position.z), 2.0f);

        // 在兩種灰色之間切換形成棋盤格
        material.color = lerp(float3(0.12f, 0.14f, 0.17f), float3(0.34f, 0.37f, 0.42f), abs(checker));

        // 設定較高 Roughness 形成霧面效果
        material.roughness = 0.72f;

        // 地板設定為非金屬材質
        material.metallic = 0.0f;
    }
    // 設定左側紅色球體材質
    else if (id == 1)
    {
        material.color = float3(0.92f, 0.17f, 0.07f);
        material.roughness = 0.28f;
        material.metallic = 0.05f;
    }
    // 設定右側金色 Box 材質
    else if (id == 2)
    {
        material.color = float3(0.95f, 0.66f, 0.18f);
        material.roughness = 0.16f;
        material.metallic = 0.92f;
    }
    // 設定移動中的藍色球體材質
    else
    {
        material.color = float3(0.08f, 0.42f, 0.95f);
        material.roughness = 0.38f;
        material.metallic = 0.25f;
    }

    // 回傳物件對應的材質
    return material;
}

// 使用 SDF Gradient 估算表面法線
float3 EstimateNormal(float3 position)
{
    // 設定計算法線時的微小採樣距離
    const float epsilon = 0.001f;

    // 建立各軸偏移量
    const float2 offset = float2(epsilon, 0.0f);

    // 分別計算 X Y Z 方向的中央差分並正規化
    return normalize(float3(
        SceneSDF(position + offset.xyy).distance - SceneSDF(position - offset.xyy).distance,
        SceneSDF(position + offset.yxy).distance - SceneSDF(position - offset.yxy).distance,
        SceneSDF(position + offset.yyx).distance - SceneSDF(position - offset.yyx).distance));
}

// 使用 Sphere Tracing 尋找 Ray 與場景的交點
ISObj RayMarch(Ray ray)
{
    // 紀錄 Ray 目前已前進的距離
    float depth = 0.0f;

    [loop]
    // 最多執行 192 次 Ray Marching
    for (int step = 0; step < 192; ++step)
    {
        // 計算 Ray 目前採樣的位置
        const float3 currentPosition = ray.origin + depth * ray.direction;

        // 查詢目前位置距離最近物體的 SDF
        const ISObj intersection = SceneSDF(currentPosition);

        // 隨 Ray 距離增加稍微放寬命中 Threshold
        const float hitThreshold = max(DIST_MIN, depth * 0.00005f);

        // 距離足夠接近表面時視為命中
        if (intersection.distance <= hitThreshold)
            return MakeISObj(depth, intersection.type, intersection.id);

        // 根據 SDF 安全距離向前步進
        depth += max(intersection.distance, hitThreshold * 0.5f);

        // 超出場景最大追蹤距離時停止
        if (depth >= DIST_MAX)
            break;
    }

    // 沒有命中任何物體
    return MakeISObj(DIST_MAX, OBJECT_NONE, OBJECT_NONE);
}

// 計算表面位置朝向光源時的陰影可見度
float ShadowVisibility(float3 origin, Light light)
{
    // 建立由表面指向光源的 Shadow Ray
    const Ray shadowRay = LightRay(origin, light);

    // 取得 Shadow Ray 最多需要追蹤的距離
    const float maximumDistance = LightDist(origin, light);

    // 從稍微離開表面的位置開始追蹤避免 Self-Intersection
    float depth = 0.02f;

    // 預設光源完全可見
    float visibility = 1.0f;

    [loop]
    // 沿 Shadow Ray 進行 SDF 採樣
    for (int step = 0; step < 96 && depth < maximumDistance; ++step)
    {
        // 計算目前 Shadow Ray 位置到場景最近表面的距離
        const float distanceToScene = SceneSDF(origin + shadowRay.direction * depth).distance;

        // 距離過小代表 Shadow Ray 被物體遮擋
        if (distanceToScene < DIST_MIN)
            return 0.0f;

        // 利用 SDF 與目前深度比例近似 Soft Shadow
        visibility = min(visibility, 18.0f * distanceToScene / depth);

        // 根據 SDF 安全距離向光源方向前進
        depth += max(distanceToScene, 0.01f);
    }

    // 將陰影可見度限制在 0 到 1
    return saturate(visibility);
}

// 計算 GGX Normal Distribution Function
float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
    // 將 Roughness 平方轉換為 GGX 使用的 alpha 並設定最小值
    const float alpha = max(roughness * roughness, 0.0025f);

    // 計算 alpha 的平方
    const float alphaSquared = alpha * alpha;

    // 計算法線與 Half Vector 的內積
    const float normalDotHalf = saturate(dot(normal, halfVector));

    // 計算 GGX Distribution 的分母中間項
    const float denominator = normalDotHalf * normalDotHalf * (alphaSquared - 1.0f) + 1.0f;

    // 回傳 GGX Distribution 並避免分母過小
    return alphaSquared / max(PI * denominator * denominator, 0.0001f);
}

// 使用 Schlick-GGX 近似計算 Geometry Term
float GeometrySchlickGGX(float normalDotDirection, float roughness)
{
    // 根據 Roughness 計算 Geometry Function 使用的 k 參數
    const float k = ((roughness + 1.0f) * (roughness + 1.0f)) / 8.0f;

    // 回傳遮蔽與幾何自遮蔽比例
    return normalDotDirection / max(normalDotDirection * (1.0f - k) + k, 0.0001f);
}

// 使用 Schlick Approximation 計算 Fresnel
float3 FresnelSchlick(float cosine, float3 reflectance)
{
    // 根據觀察角度從基礎反射率逼近 Fresnel Effect
    return reflectance + (1.0f - reflectance) * pow(1.0f - saturate(cosine), 5.0f);
}

// 使用 Cook-Torrance BRDF 計算表面的 PBR 光照
float3 PBR(HitSurface hit, Ray ray, Light light, float visibility)
{
    // 正規化表面法線
    const float3 normal = normalize(hit.normal);

    // 計算表面指向相機的 View Direction
    const float3 viewDirection = normalize(-ray.direction);

    // 取得表面指向光源的方向
    const float3 lightDirection = LightRay(hit.hitPoint, light).direction;

    // 計算 View Direction 與 Light Direction 的 Half Vector
    const float3 halfVector = normalize(viewDirection + lightDirection);

    // 根據 Metallic 在非金屬預設反射率與材質顏色之間插值
    const float3 reflectance = lerp(0.04f.xxx, hit.material.color, hit.material.metallic);

    // 計算法線與光線方向的內積
    const float normalDotLight = saturate(dot(normal, lightDirection));

    // 計算法線與視線方向的內積
    const float normalDotView = saturate(dot(normal, viewDirection));

    // 計算 GGX Normal Distribution
    const float distribution = DistributionGGX(normal, halfVector, hit.material.roughness);

    // 結合 View 與 Light 方向的 Geometry Term
    const float geometry = GeometrySchlickGGX(normalDotView, hit.material.roughness) *
                           GeometrySchlickGGX(normalDotLight, hit.material.roughness);

    // 計算目前角度下的 Fresnel 反射率
    const float3 fresnel = FresnelSchlick(dot(halfVector, viewDirection), reflectance);

    // 使用 Cook-Torrance BRDF 計算 Specular Reflection
    const float3 specular = distribution * geometry * fresnel /
                            max(4.0f * normalDotView * normalDotLight, 0.0001f);

    // 計算 Diffuse Reflection 所占比例並排除金屬材質的 Diffuse 成分
    const float3 diffuseWeight = (1.0f - fresnel) * (1.0f - hit.material.metallic);

    // 取得光源的原始強度
    float intensity = light.intensity;

    // Point Light 套用 Inverse Square Law
    if (light.type == 1)
    {
        // 計算表面到 Point Light 的距離
        const float lightDistance = LightDist(hit.hitPoint, light);

        // 讓光照強度依距離平方衰減
        intensity /= max(lightDistance * lightDistance, 0.01f);
    }

    // 根據光源顏色與衰減後強度計算 Radiance
    const float3 radiance = light.color * intensity;

    // 結合 Lambert Diffuse、Specular、入射光與陰影計算直接光照
    const float3 directLighting =
        (diffuseWeight * hit.material.color / PI + specular) * radiance * normalDotLight * visibility;

    // 加入簡單 Ambient Lighting 並依 AO 與 Metallic 調整
    const float3 ambient = hit.material.color * 0.025f * hit.material.ao * (1.0f - hit.material.metallic * 0.65f);

    // 合併 Ambient 與 Direct Lighting
    return ambient + directLighting;
}

// 根據觀察方向產生天空漸層顏色
float3 SkyColor(float3 direction)
{
    // 將方向的 Y 分量轉換為 0 到 1 的漸層係數
    const float horizon = saturate(direction.y * 0.5f + 0.5f);

    // 在深色地平線與藍色天空之間插值
    return lerp(float3(0.035f, 0.045f, 0.065f), float3(0.28f, 0.43f, 0.68f), horizon);
}

// 追蹤 Primary Ray 與 Reflection Ray 並累積最終顏色
float3 March(Ray ray, Light light)
{
    // 儲存目前累積的 HDR 顏色
    float3 accumulatedColor = 0.0f.xxx;

    // 紀錄每次 Reflection 後剩餘的能量比例
    float3 throughput = 1.0f.xxx;

    // 儲存目前正在追蹤的 Ray
    Ray currentRay = ray;

    [unroll]
    // 最多計算兩次 Ray Reflection
    for (int reflectionIndex = 0; reflectionIndex < 2; ++reflectionIndex)
    {
        // 對目前 Ray 執行 Ray Marching
        const ISObj intersection = RayMarch(currentRay);

        // 沒有命中物體時加入天空顏色
        if (intersection.type == OBJECT_NONE)
        {
            accumulatedColor += throughput * SkyColor(currentRay.direction);
            break;
        }

        // 建立命中表面的資料
        HitSurface hit;

        // 根據 Ray 距離算出實際命中位置
        hit.hitPoint = currentRay.origin + currentRay.direction * intersection.distance;

        // 使用 SDF Gradient 估算命中表面的法線
        hit.normal = EstimateNormal(hit.hitPoint);

        // 根據命中物件 ID 取得材質
        hit.material = GetMaterial(intersection.id, hit.hitPoint);

        // 從表面稍微偏移後計算 Shadow Visibility
        const float visibility = ShadowVisibility(hit.hitPoint + hit.normal * 0.01f, light);

        // 根據 Metallic 與 Roughness 計算 Reflection 所占比例
        const float reflectionAmount =
            saturate(lerp(0.08f, 0.72f, hit.material.metallic) * (1.0f - hit.material.roughness * 0.55f));

        // 將目前表面的 PBR 光照依非反射比例加入最終顏色
        accumulatedColor += throughput * PBR(hit, currentRay, light, visibility) * (1.0f - reflectionAmount);

        // 更新下一次 Reflection 剩餘的能量
        throughput *= reflectionAmount;

        // 建立沿表面反射方向前進的新 Ray
        currentRay = MakeRay(hit.hitPoint + hit.normal * 0.01f, reflect(currentRay.direction, hit.normal));
    }

    // 回傳所有 Ray 貢獻累積後的顏色
    return accumulatedColor;
}

// 根據 UV 與滑鼠位置建立相機的 Primary Ray
Ray CreateCameraRay(float2 uv)
{
    // 將滑鼠 X 座標轉換為相機水平旋轉角度
    const float yaw = (mousePos.x - 0.5f) * 1.7f;

    // 將滑鼠 Y 座標轉換為相機俯仰角度
    const float elevation = 0.20f + (0.5f - mousePos.y) * 0.75f;

    // 設定相機持續看向的場景中心
    const float3 target = float3(0.0f, -0.05f, 4.0f);

    // 設定 Orbit Camera 與目標之間的距離
    const float radius = 7.5f;

    // 根據 Yaw 與 Elevation 計算 Orbit Camera 位置
    const float3 cameraPosition = target +
        float3(sin(yaw) * cos(elevation), sin(elevation), -cos(yaw) * cos(elevation)) * radius;

    // 計算相機朝向目標的 Forward Vector
    const float3 forward = normalize(target - cameraPosition);

    // 透過 World Up 與 Forward 計算相機 Right Vector
    const float3 right = normalize(cross(float3(0.0f, 1.0f, 0.0f), forward));

    // 計算相機 Up Vector
    const float3 up = cross(forward, right);

    // 將 UV 從 0 到 1 轉換成考慮 Aspect Ratio 的 Screen Space 座標
    const float2 screenPosition = (uv * 2.0f - 1.0f) * float2(aspectRatio, -1.0f);

    // 結合相機 Basis Vector 建立穿過目前 Pixel 的 Primary Ray
    return MakeRay(cameraPosition, forward * 1.65f + right * screenPosition.x + up * screenPosition.y);
}

// 使用 SV_VertexID 產生不需要 Vertex Buffer 的 Fullscreen Triangle
VSOutput VSMain(uint vertexId : SV_VertexID)
{
    // 根據 Vertex ID 產生 Fullscreen Triangle 的頂點座標
    const float2 position = float2((vertexId << 1) & 2, vertexId & 2);

    VSOutput output;

    // 將產生的座標轉換成 Clip Space
    output.position = float4(position * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    // 將原始頂點座標作為 UV 傳給 Pixel Shader
    output.uv = position;

    return output;
}

// Pixel Shader 主函式
float4 PSMain(VSOutput input) : SV_Target
{
    // 建立場景中的主要光源
    Light light;

    // 將光源設定為 Point Light
    light.type = 1;

    // Point Light 不使用固定方向
    light.dir = 0.0f.xxx;

    // 使用時間讓 Point Light 環繞場景移動
    light.center = float3(cos(time * 0.65f) * 3.5f, 4.5f, 3.0f + sin(time * 0.65f) * 2.5f);

    // 設定 Point Light 強度
    light.intensity = 115.0f;

    // 設定偏暖色的光源顏色
    light.color = float3(1.0f, 0.82f, 0.62f);

    // 建立目前 Pixel 對應的 Camera Ray
    const Ray cameraRay = CreateCameraRay(input.uv);

    // 執行 Ray Marching 與 PBR 計算取得 HDR 顏色
    const float3 hdrColor = March(cameraRay, light);

    // 使用 Reinhard Tone Mapping 將 HDR 顏色映射到較低動態範圍
    const float3 toneMapped = hdrColor / (hdrColor + 1.0f);

    // 執行 Gamma Correction 並限制輸入範圍
    const float3 gammaCorrected = pow(saturate(toneMapped), 1.0f / 2.2f);

    // 輸出不透明的最終 Pixel 顏色
    return float4(gammaCorrected, 1.0f);
}