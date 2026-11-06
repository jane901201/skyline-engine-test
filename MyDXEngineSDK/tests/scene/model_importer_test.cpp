#include "core/model_importer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace Skyline;
using Catch::Approx;

namespace
{
// 把文字寫成暫存檔，離開作用域時刪除
class TempFile
{
  public:
    TempFile(const std::string& name, const std::string& content)
        : m_path(std::filesystem::temp_directory_path() / name)
    {
        std::ofstream(m_path) << content;
    }
    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }
    [[nodiscard]] const std::filesystem::path& Path() const
    {
        return m_path;
    }

  private:
    std::filesystem::path m_path;
};

// 每個節點的父節點都必須排在它前面，建立場景時才能一路往下建
void CheckNodeOrder(const ImportedModel& model)
{
    for (std::size_t i = 0; i < model.nodes.size(); ++i)
    {
        const ImportedNode& node = model.nodes[i];
        CHECK(node.parent < static_cast<int>(i));
        CHECK(node.mesh < static_cast<int>(model.meshes.size()));
    }
    REQUIRE_FALSE(model.nodes.empty());
    CHECK(model.nodes.front().parent == -1);
}
} // namespace

TEST_CASE("A triangle OBJ imports with its bounds and node order", "[import]")
{
    const TempFile file("skyline_import_triangle.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    const ImportedModel model = ImportModelFile(file.Path());

    REQUIRE(model.meshes.size() == 1);
    CHECK(model.meshes[0].geometry.vertices.size() == 3);
    CHECK(model.meshes[0].geometry.indices.size() == 3);
    CheckNodeOrder(model);

    bool referenced = false;
    for (const ImportedNode& node : model.nodes)
        referenced = referenced || node.mesh == 0;
    CHECK(referenced);

    const AABB& bounds = model.meshes[0].bounds;
    REQUIRE(bounds.IsValid());
    CHECK(bounds.Center().x == Approx(0.5f).margin(1e-4f));
    CHECK(bounds.Center().y == Approx(0.5f).margin(1e-4f));
    CHECK(bounds.Extents().x == Approx(0.5f).margin(1e-4f));
    CHECK(bounds.Extents().y == Approx(0.5f).margin(1e-4f));
}

TEST_CASE("Imported normals are generated when the file has none", "[import]")
{
    const TempFile file("skyline_import_normals.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    const ImportedModel model = ImportModelFile(file.Path());
    for (const MeshVertex& vertex : model.meshes.at(0).geometry.vertices)
    {
        const float lengthSq = vertex.normal.x * vertex.normal.x + vertex.normal.y * vertex.normal.y +
                               vertex.normal.z * vertex.normal.z;
        CHECK(lengthSq == Approx(1.0f).margin(1e-3f));
    }
}

TEST_CASE("The repository Cube.fbx imports as a valid hierarchy", "[import]")
{
    const std::filesystem::path path = std::filesystem::path(SKYLINE_ASSET_DIR) / "fbx" / "Cube.fbx";
    const ImportedModel model = ImportModelFile(path);

    REQUIRE_FALSE(model.meshes.empty());
    CheckNodeOrder(model);
    for (const ImportedMesh& mesh : model.meshes)
    {
        CHECK_FALSE(mesh.geometry.vertices.empty());
        CHECK(mesh.geometry.indices.size() % 3 == 0);
        CHECK(mesh.bounds.IsValid());
        for (const std::uint32_t index : mesh.geometry.indices)
            CHECK(index < mesh.geometry.vertices.size());
    }
}

TEST_CASE("Importing rejects bad input", "[import]")
{
    CHECK_THROWS_AS(ImportModelFile(std::filesystem::path()), std::invalid_argument);
    CHECK_THROWS_AS(ImportModelFile(std::filesystem::temp_directory_path() / "skyline_missing_model.fbx"),
                    std::runtime_error);

    // 只有線、沒有三角形的檔案
    const TempFile lines("skyline_import_lines.obj", "v 0 0 0\nv 1 0 0\nl 1 2\n");
    CHECK_THROWS_AS(ImportModelFile(lines.Path()), std::runtime_error);
}

TEST_CASE("FBX units are converted to meters", "[import]")
{
    // 範例 FBX 的單位是公分，網格節點帶著 100 倍縮放，根節點由匯入器補上 0.01。
    // 沿著階層把縮放乘起來，Cube 的半邊長應該是 1 公尺（與程式碼產生的立方體一致，邊長 2）
    for (const char* name : {"Cube.fbx", "Sphere.fbx"})
    {
        const std::filesystem::path path = std::filesystem::path(SKYLINE_ASSET_DIR) / "fbx" / name;
        const ImportedModel model = ImportModelFile(path);

        for (const ImportedNode& node : model.nodes)
        {
            if (node.mesh < 0)
                continue;
            float scale = 1.0f;
            for (int walk = static_cast<int>(&node - model.nodes.data()); walk >= 0; walk = model.nodes[walk].parent)
                scale *= model.nodes[static_cast<std::size_t>(walk)].local.scale.x;
            const float halfSize = scale * model.meshes[static_cast<std::size_t>(node.mesh)].bounds.Extents().x;
            INFO(name);
            CHECK(halfSize == Approx(1.0f).margin(1e-3f));
        }
    }
}
