#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

// 定義 Compute Shader 與 Render Shader 共用的粒子資料格式
struct Particle
{
    // 儲存粒子在世界空間中的位置
    float3 position;

    // 儲存粒子的剩餘生命時間
    float life;
    
    // 儲存粒子的移動方向
    float3 velocity;

    // 保留欄位以維持結構的記憶體對齊
    float padding;
};

#endif