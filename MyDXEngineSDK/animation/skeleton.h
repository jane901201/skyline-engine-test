#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "animation/animation_clip.h"
#include "math/matrix.h"

namespace Skyline
{
/// 一個骨架關節
struct Joint
{
    std::string name;
    int parent = -1;       ///< 父關節的索引；根關節為 -1
    Matrix inverseBind;    ///< 綁定姿勢下，從模型空間到關節空間的矩陣
};

/// <summary>
/// 骨架：關節陣列。父關節必須排在子關節之前，所以一次由前往後掃描就能算出所有模型空間矩陣。
/// </summary>
class Skeleton
{
  public:
    /// 名稱重複、父索引不合法（需小於自己）時丟出 std::invalid_argument
    explicit Skeleton(std::vector<Joint> joints) : m_joints(std::move(joints))
    {
        for (std::size_t i = 0; i < m_joints.size(); ++i)
        {
            const Joint& joint = m_joints[i];
            if (joint.name.empty())
                throw std::invalid_argument("Skeleton: joint " + std::to_string(i) + " has an empty name.");
            if (joint.parent < -1 || joint.parent >= static_cast<int>(i))
                throw std::invalid_argument("Skeleton: joint '" + joint.name + "' must come after its parent.");
            for (std::size_t j = 0; j < i; ++j)
                if (m_joints[j].name == joint.name)
                    throw std::invalid_argument("Skeleton: duplicate joint name '" + joint.name + "'.");
        }
    }

    [[nodiscard]] std::size_t JointCount() const noexcept
    {
        return m_joints.size();
    }
    [[nodiscard]] const Joint& GetJoint(std::size_t index) const
    {
        return m_joints.at(index);
    }

    /// 以名稱找關節索引；找不到時丟出 std::invalid_argument
    [[nodiscard]] std::size_t IndexOf(const std::string& name) const
    {
        for (std::size_t i = 0; i < m_joints.size(); ++i)
            if (m_joints[i].name == name)
                return i;
        throw std::invalid_argument("Skeleton::IndexOf: no joint named '" + name + "'.");
    }

    /// 每個關節的本地姿勢。這裡用作所有關節都沒有動畫時的預設值（由呼叫端設定綁定姿勢）
    using Pose = std::vector<Transform>;

    /// <summary>
    /// 把 clip 取樣到 pose：每個通道以名稱對應到關節，只覆寫該通道有曲線的分量。
    /// </summary>
    /// <remarks>pose 的大小必須等於關節數；通道找不到對應關節時丟出 std::invalid_argument。</remarks>
    void Sample(const AnimationClip& clip, float time, bool loop, Pose& pose) const
    {
        RequirePoseSize(pose);
        const float sampleTime = clip.WrapTime(time, loop);
        for (const AnimationChannel& channel : clip.channels)
        {
            const std::size_t index = IndexOf(channel.target);
            pose[index] = channel.Apply(pose[index], sampleTime);
        }
    }

    /// <summary>
    /// 由本地姿勢算出每個關節的模型空間矩陣（local * parent）。
    /// </summary>
    [[nodiscard]] std::vector<Matrix> ModelMatrices(const Pose& pose) const
    {
        RequirePoseSize(pose);
        std::vector<Matrix> model(m_joints.size());
        for (std::size_t i = 0; i < m_joints.size(); ++i)
        {
            const Matrix local = pose[i].ToMatrix();
            model[i] = m_joints[i].parent < 0 ? local : local * model[static_cast<std::size_t>(m_joints[i].parent)];
        }
        return model;
    }

    /// <summary>
    /// 蒙皮矩陣 = inverseBind * 模型空間矩陣（row-vector：v * inverseBind * model）。
    /// 綁定姿勢下每個蒙皮矩陣都是單位矩陣。可直接上傳給 HLSL 的 row_major 矩陣陣列，不需要轉置。
    /// </summary>
    [[nodiscard]] std::vector<Matrix> SkinningMatrices(const Pose& pose) const
    {
        std::vector<Matrix> result = ModelMatrices(pose);
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = m_joints[i].inverseBind * result[i];
        return result;
    }

  private:
    void RequirePoseSize(const Pose& pose) const
    {
        if (pose.size() != m_joints.size())
            throw std::invalid_argument("Skeleton: pose size does not match the joint count.");
    }

    std::vector<Joint> m_joints;
};
} // namespace Skyline
