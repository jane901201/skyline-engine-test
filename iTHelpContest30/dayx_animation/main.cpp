// 動畫範例：使用 Skyline 的 AnimationClip / Animator / Skeleton，在主控台顯示結果。
// 內容：場景節點的關鍵影格動畫、播放控制（循環、速度、暫停、倒放），以及骨架的蒙皮矩陣。

#include "animation/animator.h"
#include "animation/skeleton.h"

#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>

using namespace Skyline;

namespace
{
Quaternion RotationZ(float radians)
{
    Quaternion q;
    DirectX::XMStoreFloat4(&q.vec, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 0, 1, 0), radians));
    return q;
}

Transform At(float x, float y, float z)
{
    Transform t;
    t.position = Vector3(x, y, z);
    return t;
}

void PrintVec(const char* label, const Vector3& v)
{
    std::printf("%-14s (%6.2f, %6.2f, %6.2f)\n", label, v.x, v.y, v.z);
}

// 一條橫軸 0..10 的水平長條，用 '#' 標出 x 的位置
void DrawBar(float x)
{
    constexpr int width = 41;
    char line[width + 1];
    for (int i = 0; i < width; ++i)
        line[i] = '.';
    line[width] = '\0';
    const int column = static_cast<int>(std::lround(x / 10.0f * (width - 1)));
    if (column >= 0 && column < width)
        line[column] = '#';
    std::printf("    |%s|\n", line);
}
} // namespace

int main()
{
    try
    {
        // ----- 1. 場景節點動畫：平台沿 X 滑動，箱子在平台上持續升降 -----
        Scene scene;
        const EntityId root = scene.CreateEntity("root");
        const EntityId platform = scene.CreateEntity("platform", root);
        const EntityId crate = scene.CreateEntity("crate", platform);
        scene.SetLocalTransform(crate, At(0, 1, 0));

        auto clip = std::make_shared<AnimationClip>();
        clip->name = "slide and bob";
        clip->duration = 4.0f;

        AnimationChannel slide;
        slide.target = "platform";
        slide.position.keys = {{0.0f, Vector3(0, 0, 0)}, {2.0f, Vector3(10, 0, 0)}, {4.0f, Vector3(0, 0, 0)}};
        AnimationChannel bob;
        bob.target = "crate";
        bob.position.keys = {{0.0f, Vector3(0, 1, 0)}, {1.0f, Vector3(0, 2, 0)}, {2.0f, Vector3(0, 1, 0)},
                             {3.0f, Vector3(0, 2, 0)}, {4.0f, Vector3(0, 1, 0)}};
        bob.position.interpolation = Interpolation::Step; // 箱子用階梯式內插，跳著變化，和平台的平滑移動對照
        clip->channels = {slide, bob};

        Animator animator(scene, root);
        animator.Play(clip, true);

        std::printf("[1] Clip \"%s\" (%.0f s, looping): platform slides, crate steps up and down\n",
                    clip->name.c_str(), clip->duration);
        for (int i = 0; i <= 8; ++i)
        {
            std::printf("  t = %.1f s\n", animator.Time());
            DrawBar(scene.WorldPosition(platform).x);
            std::printf("    ");
            PrintVec("platform", scene.WorldPosition(platform));
            std::printf("    ");
            PrintVec("crate", scene.WorldPosition(crate));
            animator.Update(0.5f);
        }

        // ----- 2. 播放控制 -----
        std::printf("\n[2] Playback control (platform x)\n");
        animator.SetTime(1.0f);
        std::printf("  SetTime(1.0)          x = %5.2f\n", scene.WorldPosition(platform).x);
        animator.SetSpeed(2.0f);
        animator.Update(0.5f);
        std::printf("  speed 2x, +0.5 s      x = %5.2f\n", scene.WorldPosition(platform).x);
        animator.Pause();
        animator.Update(1.0f);
        std::printf("  paused, +1.0 s        x = %5.2f\n", scene.WorldPosition(platform).x);
        animator.Resume();
        animator.SetSpeed(-1.0f);
        animator.Update(0.5f);
        std::printf("  reverse, +0.5 s       x = %5.2f\n", scene.WorldPosition(platform).x);

        animator.Play(clip, false);
        animator.SetSpeed(1.0f);
        animator.Update(10.0f);
        std::printf("  non-looping, +10 s    x = %5.2f  finished = %s\n", scene.WorldPosition(platform).x,
                    animator.IsFinished() ? "yes" : "no");

        // ----- 3. 骨架與蒙皮矩陣：三節手臂 -----
        std::printf("\n[3] Skeleton: 3-joint arm, bind pose has the arm pointing up (+Y)\n");
        const Skeleton::Pose bindPose = {At(0, 0, 0), At(0, 2, 0), At(0, 2, 0)};
        std::vector<Joint> joints(3);
        const char* names[] = {"shoulder", "elbow", "wrist"};
        Matrix bindWorld = Matrix::Identity;
        for (int i = 0; i < 3; ++i)
        {
            bindWorld = i == 0 ? bindPose[0].ToMatrix() : bindPose[i].ToMatrix() * bindWorld;
            joints[i].name = names[i];
            joints[i].parent = i - 1;
            joints[i].inverseBind = bindWorld;
            joints[i].inverseBind.Inverse();
        }
        const Skeleton skeleton(joints);

        AnimationClip bend;
        bend.name = "bend elbow";
        bend.duration = 1.0f;
        AnimationChannel elbowBend;
        elbowBend.target = "elbow";
        elbowBend.position.keys = {{0.0f, Vector3(0, 2, 0)}};
        elbowBend.rotation.keys = {{0.0f, RotationZ(0.0f)}, {1.0f, RotationZ(DirectX::XM_PIDIV2)}};
        bend.channels = {elbowBend};

        for (const float time : {0.0f, 0.5f, 1.0f})
        {
            Skeleton::Pose pose = bindPose;
            skeleton.Sample(bend, time, false, pose);
            const std::vector<Matrix> model = skeleton.ModelMatrices(pose);
            const std::vector<Matrix> skin = skeleton.SkinningMatrices(pose);

            // 手腕的模型空間位置，以及把綁定姿勢的手腕頂點 (0,4,0) 經蒙皮矩陣後的位置
            Vector3 wristJoint(0, 0, 0);
            model[2].Apply(wristJoint);
            Vector3 skinnedVertex(0, 4, 0);
            skin[2].Apply(skinnedVertex);

            std::printf("  t = %.1f s\n", time);
            std::printf("    ");
            PrintVec("wrist joint", wristJoint);
            std::printf("    ");
            PrintVec("skinned vertex", skinnedVertex);
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
