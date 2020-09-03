#include <glow-extras/glfw/GlfwContext.hh>
#include <glow-extras/viewer/view.hh>
#include <LayoutEmbedding/Assert.hh>
#include <LayoutEmbedding/Embedding.hh>
#include <LayoutEmbedding/PathStraightening.hh>
#include <LayoutEmbedding/Visualization/Visualization.hh>

using namespace LayoutEmbedding;

namespace
{

void straighten(
        const std::filesystem::path& _path_prefix)
{
    // Load layout embedding from file
    EmbeddingInput input;
    Embedding em_orig(input);
    LE_ASSERT(em_orig.load_embedding(_path_prefix));

    {
        auto g = gv::grid();
        auto style = default_style();

        // Compare results after different numbers of iterations
        for (int i = 0; i < 3; ++i)
            view_target(straighten_paths(em_orig, i));
    }
}

}

int main()
{
    namespace fs = std::filesystem;
    glow::glfw::GlfwContext ctx;

    { // SHREC
        const auto dir = fs::path(LE_OUTPUT_PATH) / "shrec07_results" / "saved_embeddings";

        straighten(dir / "384_bnb"); // Wolf
//        straighten(dir / "3_greedy"); // Human
    }

    { // Sphere
        const auto dir = fs::path(LE_OUTPUT_PATH) / "sphere_stress";

//        straighten(dir / "sphere_greedy");
    }
}
