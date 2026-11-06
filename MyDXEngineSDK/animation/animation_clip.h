#pragma once

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "math/vector.h"
#include "scene/transform.h"

namespace Skyline
{
/// 兩個關鍵影格之間的內插方式
enum class Interpolation
{
    Linear, ///< 位置、縮放用線性內插；旋轉用球面線性內插（走最短路徑）
    Step    ///< 維持前一個關鍵影格的值，直到下一個關鍵影格才跳變
};

/// 單一關鍵影格
template <typename T> struct Keyframe
{
    float time = 0.0f; ///< 秒
    T value{};
};

namespace Detail
{
inline Vector3 Blend(const Vector3& a, const Vector3& b, float t)
{
    Vector3 result;
    DirectX::XMStoreFloat3(&result.vec, DirectX::XMVectorLerp(a, b, t));
    return result;
}

inline Quaternion Blend(const Quaternion& a, const Quaternion& b, float t)
{
    Quaternion result;
    // XMQuaternionSlerp 會依內積正負號翻轉方向，所以走的是最短路徑
    DirectX::XMStoreFloat4(&result.vec, DirectX::XMQuaternionNormalize(DirectX::XMQuaternionSlerp(a, b, t)));
    return result;
}
} // namespace Detail

/// <summary>
/// 一條依時間排序的關鍵影格曲線。
/// </summary>
/// <remarks>
/// 時間必須是有限、非負且嚴格遞增。呼叫 Sample 之前請先用 Validate 檢查過（AnimationClip::Validate 會一併檢查）。
/// </remarks>
template <typename T> struct Track
{
    Interpolation interpolation = Interpolation::Linear;
    std::vector<Keyframe<T>> keys;

    [[nodiscard]] bool Empty() const noexcept
    {
        return keys.empty();
    }

    /// 最後一個關鍵影格的時間；沒有關鍵影格時為 0
    [[nodiscard]] float EndTime() const noexcept
    {
        return keys.empty() ? 0.0f : keys.back().time;
    }

    /// 時間不合法時丟出 std::invalid_argument
    void Validate(const std::string& label) const
    {
        float previous = -1.0f;
        for (const Keyframe<T>& key : keys)
        {
            if (!std::isfinite(key.time) || key.time < 0.0f)
                throw std::invalid_argument("Track " + label + ": key time must be finite and non-negative.");
            if (key.time <= previous)
                throw std::invalid_argument("Track " + label + ": key times must be strictly increasing.");
            previous = key.time;
        }
    }

    /// <summary>
    /// 取樣某個時間點的值。時間在範圍外時使用第一個或最後一個關鍵影格的值。
    /// </summary>
    /// <remarks>Track 不可為空（Empty() 為 true 時不可呼叫）。</remarks>
    [[nodiscard]] T Sample(float time) const
    {
        if (keys.size() == 1 || time <= keys.front().time)
            return keys.front().value;
        if (time >= keys.back().time)
            return keys.back().value;

        // 第一個時間大於 time 的關鍵影格；前面已排除範圍外，所以 next 不會是 begin 或 end
        const auto next = std::upper_bound(keys.begin(), keys.end(), time,
                                           [](float t, const Keyframe<T>& key) { return t < key.time; });
        const auto previous = next - 1;
        if (interpolation == Interpolation::Step)
            return previous->value;

        const float alpha = (time - previous->time) / (next->time - previous->time);
        return Detail::Blend(previous->value, next->value, alpha);
    }
};

/// <summary>
/// 一個目標節點（以名稱指定）的位置、旋轉、縮放曲線。沒有曲線的分量保持不變。
/// </summary>
struct AnimationChannel
{
    std::string target; ///< 目標節點名稱（場景實體名稱或骨架關節名稱）
    Track<Vector3> position;
    Track<Quaternion> rotation;
    Track<Vector3> scale;

    /// 取樣 time，只覆寫有曲線的分量，其餘沿用 base
    [[nodiscard]] Transform Apply(const Transform& base, float time) const
    {
        Transform result = base;
        if (!position.Empty())
            result.position = position.Sample(time);
        if (!rotation.Empty())
            result.rotation = rotation.Sample(time);
        if (!scale.Empty())
            result.scale = scale.Sample(time);
        return result;
    }
};

/// <summary>
/// 一段動畫：多個通道與總長度。
/// </summary>
struct AnimationClip
{
    std::string name;
    float duration = 0.0f; ///< 秒，必須大於 0
    std::vector<AnimationChannel> channels;

    /// <summary>
    /// 檢查資料是否合法（長度、目標名稱、關鍵影格時間與四元數）。不合法時丟出 std::invalid_argument。
    /// </summary>
    void Validate() const
    {
        if (!std::isfinite(duration) || duration <= 0.0f)
            throw std::invalid_argument("AnimationClip " + name + ": duration must be greater than 0.");

        for (const AnimationChannel& channel : channels)
        {
            if (channel.target.empty())
                throw std::invalid_argument("AnimationClip " + name + ": a channel has an empty target.");
            const std::string label = name + "/" + channel.target;
            channel.position.Validate(label + "/position");
            channel.rotation.Validate(label + "/rotation");
            channel.scale.Validate(label + "/scale");

            const float end = std::max({channel.position.EndTime(), channel.rotation.EndTime(), channel.scale.EndTime()});
            if (end > duration)
                throw std::invalid_argument("AnimationClip " + label + ": a key is later than the clip duration.");

            for (const Keyframe<Quaternion>& key : channel.rotation.keys)
            {
                const float lengthSq = key.value.vec.x * key.value.vec.x + key.value.vec.y * key.value.vec.y +
                                       key.value.vec.z * key.value.vec.z + key.value.vec.w * key.value.vec.w;
                if (std::fabs(lengthSq - 1.0f) > 1e-3f)
                    throw std::invalid_argument("AnimationClip " + label + ": rotation keys must be unit quaternions.");
            }
        }
    }

    /// 將播放時間轉成 [0, duration] 內的取樣時間：循環時取餘數，否則夾在範圍內
    [[nodiscard]] float WrapTime(float time, bool loop) const noexcept
    {
        if (loop)
        {
            const float wrapped = std::fmod(time, duration);
            return wrapped < 0.0f ? wrapped + duration : wrapped;
        }
        return std::clamp(time, 0.0f, duration);
    }
};
} // namespace Skyline
