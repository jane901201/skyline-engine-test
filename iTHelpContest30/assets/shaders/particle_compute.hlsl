#include "particle_common.hlsli"

// 宣告可由 Compute Shader 讀寫的粒子 StructuredBuffer 並綁定至 u0
RWStructuredBuffer<Particle> particles : register(u0);

// 宣告粒子模擬使用的常數緩衝區並綁定至 b0
cbuffer ParticleSimulationConstants : register(b0)
{
    // 儲存目前 Frame 經過的時間
    float deltaTime;

    // 儲存目前實際使用的粒子數量
    uint particleCount;

    // 儲存粒子 Buffer 可容納的最大粒子數量
    uint particleCapacity;

    // 儲存粒子活動空間的邊界大小
    float boundarySize;

    // 儲存粒子的移動速度倍率
    float speed;

    // 儲存新粒子的基礎生命週期
    float lifetime;

    // 控制是否重新初始化所有粒子
    uint resetParticles;

    // 儲存產生隨機數時使用的 Seed
    uint randomSeed;
};

// 將輸入整數經過多次位元運算與乘法混合產生 Hash 值
uint Hash(uint value)
{
    // 將高位元資訊混合至低位元
    value ^= value >> 16;

    // 使用常數乘法進一步打散位元分布
    value *= 0x7feb352du;

    // 再次混合不同區段的位元
    value ^= value >> 15;

    // 使用另一組常數降低 Hash 結果的規律性
    value *= 0x846ca68bu;

    // 最後再次混合高低位元
    value ^= value >> 16;

    // 回傳完成混合後的 Hash 值
    return value;
}

// 根據傳入狀態產生 0 到 1 之間的偽隨機浮點數
float Random01(inout uint state)
{
    // 更新隨機狀態避免連續取得相同結果
    state = Hash(state);

    // 取低 24 位元並正規化至 0 到 1 的範圍
    return (state & 0x00ffffffu) / 16777216.0f;
}

// 根據粒子索引建立具有隨機位置與方向的新粒子
Particle CreateParticle(uint particleIndex)
{
    // 結合粒子索引與全域 Seed 建立此粒子的初始隨機狀態
    uint state = Hash(particleIndex ^ randomSeed);

    // 產生每個分量皆位於負 1 到正 1 的隨機位置
    float3 position = float3(Random01(state), Random01(state), Random01(state)) * 2.0f - 1.0f;

    // 產生每個分量皆位於負 1 到正 1 的隨機移動方向
    float3 direction = float3(Random01(state), Random01(state), Random01(state)) * 2.0f - 1.0f;

    // 檢查方向向量的平方長度是否過度接近零
    if (dot(direction, direction) < 0.0001f)

        // 使用向上的單位方向避免 normalize 產生不穩定結果
        direction = float3(0.0f, 1.0f, 0.0f);

    // 建立準備初始化的粒子資料
    Particle particle;

    // 將隨機位置限制在邊界大小的百分之八十範圍內
    particle.position = position * boundarySize * 0.8f;

    // 將生命週期隨機設定為基礎生命週期的百分之二十五到百分之一百
    particle.life = lifetime * (0.25f + Random01(state) * 0.75f);

    // 將隨機方向正規化後設為粒子的移動方向
    particle.velocity = normalize(direction);

    // 將用於記憶體對齊的欄位初始化為零
    particle.padding = 0.0f;

    // 回傳完成初始化的粒子
    return particle;
}

// 設定每個 Thread Group 使用 64 個 X 軸執行緒
[numthreads(64, 1, 1)]

// Compute Shader 的主要進入點
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 以目前 Dispatch Thread 的 X 編號作為粒子索引
    const uint particleIndex = dispatchThreadID.x;

    // Reset 時處理整個 Buffer 否則只處理目前有效粒子
    const uint dispatchCount = resetParticles != 0 ? particleCapacity : particleCount;

    // 排除因 Thread Group 向上取整而超出有效範圍的執行緒
    if (particleIndex >= dispatchCount)

        // 結束目前執行緒避免存取超出 Buffer 範圍
        return;

    // 判斷目前是否要求重新初始化粒子
    if (resetParticles != 0)
    {
        // 重新建立目前索引對應的粒子
        particles[particleIndex] = CreateParticle(particleIndex);

        // Reset 完成後不執行後續模擬
        return;
    }

    // 從 StructuredBuffer 讀取目前粒子的資料
    Particle particle = particles[particleIndex];

    // 根據方向速度與 Frame 時間更新粒子位置
    particle.position += particle.velocity * speed * deltaTime;

    // 檢查粒子是否超出 X 軸邊界
    if (particle.position.x > boundarySize || particle.position.x < -boundarySize)
    {
        // 將 X 座標限制回合法邊界範圍
        particle.position.x = clamp(particle.position.x, -boundarySize, boundarySize);

        // 反轉 X 軸速度形成碰撞反彈效果
        particle.velocity.x *= -1.0f;
    }

    // 檢查粒子是否超出 Y 軸邊界
    if (particle.position.y > boundarySize || particle.position.y < -boundarySize)
    {
        // 將 Y 座標限制回合法邊界範圍
        particle.position.y = clamp(particle.position.y, -boundarySize, boundarySize);

        // 反轉 Y 軸速度形成碰撞反彈效果
        particle.velocity.y *= -1.0f;
    }

    // 檢查粒子是否超出 Z 軸邊界
    if (particle.position.z > boundarySize || particle.position.z < -boundarySize)
    {
        // 將 Z 座標限制回合法邊界範圍
        particle.position.z = clamp(particle.position.z, -boundarySize, boundarySize);

        // 反轉 Z 軸速度形成碰撞反彈效果
        particle.velocity.z *= -1.0f;
    }

    // 扣除目前 Frame 經過的時間
    particle.life -= deltaTime;

    // 檢查粒子的生命週期是否已結束
    if (particle.life <= 0.0f)
    {
        // 使用額外混合過的索引重新產生粒子避免固定重複相同序列
        particle = CreateParticle(particleIndex + randomSeed * 1664525u);

        // 將重新生成的粒子移動至世界原點
        particle.position = float3(0.0f, 0.0f, 0.0f);
    }

    // 將更新完成的粒子資料寫回 StructuredBuffer
    particles[particleIndex] = particle;
}