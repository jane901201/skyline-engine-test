#include "material.h"

// 使用材質常數初始化 Material 與對應的 Constant Buffer
void Material::init(const MaterialConstants& constants)
{
    // 保存目前材質使用的常數資料
    m_constants = constants;

    // 建立 Constant Buffer 並寫入初始材質常數
    m_constantBuffer.init(sizeof(MaterialConstants), &m_constants);
}

// 更新材質常數並同步到 GPU
void Material::setConstants(const MaterialConstants& constants)
{
    // 更新 CPU 端保存的材質常數
    m_constants = constants;

    // 將最新的材質常數複製到 VRAM
    m_constantBuffer.copyToVRAM(m_constants);
}

// 取得材質 Constant Buffer 的 GPU Virtual Address
D3D12_GPU_VIRTUAL_ADDRESS Material::getConstantsAddress() const
{
    // 回傳 Constant Buffer 在 GPU 上的虛擬位址
    return m_constantBuffer.getGPUVirtualAddress();
}