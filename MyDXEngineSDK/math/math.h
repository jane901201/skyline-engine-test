#pragma once

class Math
{
  public:
    // 圓周率
    static constexpr float PI = 3.14159265358979323846f;
    // 圓周率的平方
    static constexpr float PI2 = PI * 2.0f;

    /// <summary>
    /// 將角度從 Degree 單位轉換為 Radian 單位。
    /// </summary>
    /// <param name="deg">以 Degree 為單位的角度。</param>
    /// <returns>以 Radian 為單位的角度。</returns>
    constexpr static inline float DegToRad(float deg)
    {
        return deg * (PI / 180.0f);
    }
    /// <summary>
    /// 將角度從 Radian 單位轉換為 Degree 單位。
    /// </summary>
    /// <param name="rad">以 Radian 為單位的角度。</param>
    /// <returns>以 Degree 為單位的角度。</returns>
    constexpr static inline float RadToDeg(float rad)
    {
        return rad / (PI / 180.0f);
    }
    /// <summary>
    /// 在 t0 與 t1 之間進行線性插值。
    /// </summary>
    /// <param name="rate">插值比例為 0.0～1.0</param>
    /// <param name="t0">插值起始值。</param>
    /// <param name="t1">插值結束值。</param>
    /// <returns>插值後的值。</returns>
    template <class T> static inline T Lerp(float rate, T t0, T t1)
    {
        T ret;
        ret.Lerp(rate, t0, t1);
        return ret;
    }
    /// <summary>
    /// 在 t0 與 t1 之間進行線性插值。
    /// </summary>
    /// <param name="rate">插值比例為 0.0～1.0</param>
    /// <param name="t0">插值起始值。</param>
    /// <param name="t1">插值結束值。</param>
    /// <returns>插值後的值。</returns>
    template <> static inline float Lerp(float rate, float t0, float t1)
    {
        return t0 + (t1 - t0) * rate;
    }
};
