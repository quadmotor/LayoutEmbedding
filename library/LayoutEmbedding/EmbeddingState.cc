#include "EmbeddingState.hh"

#include <LayoutEmbedding/Assert.hh>
#include <LayoutEmbedding/UnionFind.hh>
#include <LayoutEmbedding/VirtualPathConflictSentinel.hh>

namespace LayoutEmbedding {

EmbeddingState::EmbeddingState(const Embedding& _em) :
    em(_em),
    candidate_paths(_em.layout_mesh())
{
}

void EmbeddingState::extend(const pm::edge_index& _l_ei)
{
    const auto& l_e = em.layout_mesh().edges()[_l_ei];
    LE_ASSERT(embedded_l_edges.count(l_e) == 0);

    auto l_he = l_e.halfedgeA();
    auto path = em.find_shortest_path(l_he);
    if (path.empty()) {
        embedded_cost = std::numeric_limits<double>::infinity();
        valid = false;
        return;
    }
    else {
        embedded_cost += em.path_length(path);
        em.embed_path(l_he, path);
        embedded_l_edges.insert(l_e);
    }
}

void EmbeddingState::extend(const pm::edge_index& _l_ei, const VirtualPath& _path)
{
    const auto& l_e = em.layout_mesh().edges()[_l_ei];
    LE_ASSERT(embedded_l_edges.count(l_e) == 0);

    LE_ASSERT(_path.size() >= 2);

    auto l_he = l_e.halfedgeA();
    LE_ASSERT(is_real_vertex(_path.front()));
    LE_ASSERT(is_real_vertex(_path.back()));
    LE_ASSERT(real_vertex(_path.front()) == em.matching_target_vertex(l_he.vertex_from()));
    LE_ASSERT(real_vertex(_path.back())  == em.matching_target_vertex(l_he.vertex_to()));

    embedded_cost += em.path_length(_path);
    em.embed_path(l_he, _path);
    embedded_l_edges.insert(l_e);
}

void EmbeddingState::extend(const InsertionSequence& _seq)
{
    for (const auto& l_e : _seq) {
        extend(l_e);
        if (!valid) {
            break;
        }
    }
}

void EmbeddingState::compute_candidate_paths()
{
    const Embedding& c_em = em; // We don't want to modify the embedding in this method.

    LE_ASSERT(&candidate_paths.mesh() == &em.layout_mesh());
    candidate_paths.clear();
    unembedded_cost = 0.0;

    for (const auto l_e : c_em.layout_mesh().edges()) {
        if (!embedded_l_edges.count(l_e)) {
            auto l_he = l_e.halfedgeA();
            auto path = c_em.find_shortest_path(l_he);

            candidate_paths[l_e].path = path;
            if (path.empty()) {
                candidate_paths[l_e].cost = std::numeric_limits<double>::infinity();
                valid = false;
            }
            else {
                candidate_paths[l_e].cost = c_em.path_length(path);
            }
            unembedded_cost += candidate_paths[l_e].cost;
        }
    }
}

void EmbeddingState::detect_candidate_path_conflicts()
{
    const Embedding& c_em = em; // We don't want to modify the embedding in this method.

    conflicting_l_edges.clear();
    non_conflicting_l_edges.clear();

    if (valid) {
        VirtualPathConflictSentinel vpcs(c_em);
        for (const auto l_e : c_em.layout_mesh().edges()) {
            if (!embedded_l_edges.count(l_e)) {
                const auto& path = candidate_paths[l_e].path;
                if (!path.empty()) {
                    vpcs.insert_path(path, l_e);
                }
            }
        }
        vpcs.check_path_ordering();
        conflicting_l_edges = vpcs.global_conflicts;
    }

    for (const auto l_e : c_em.layout_mesh().edges()) {
        if (!embedded_l_edges.count(l_e) && !conflicting_l_edges.count(l_e)) {
            non_conflicting_l_edges.insert(l_e);
        }
    }
    LE_ASSERT(embedded_l_edges.size() + conflicting_l_edges.size() + non_conflicting_l_edges.size() == em.layout_mesh().edges().size());
}

double EmbeddingState::cost_lower_bound() const
{
    return embedded_cost + unembedded_cost;
}

HashValue EmbeddingState::hash() const
{
    HashValue h = 0;
    for (const auto l_e : em.layout_mesh().edges()) {
        if (em.is_embedded(l_e)) {
            const auto& path = em.get_embedded_path(l_e.halfedgeA());
            for (const auto& t_v : path) {
                const auto& pos = em.target_pos()[t_v];
                h = hash_combine(h, LayoutEmbedding::hash(pos));
            }
        }
    }
    return h;
}

int EmbeddingState::count_connected_components() const
{
    int num_components = em.layout_mesh().faces().size();
    UnionFind face_components(num_components);
    for (const auto l_e : em.layout_mesh().edges()) {
        if (!em.is_embedded(l_e)) {
            const int id_A = l_e.faceA().idx.value;
            const int id_B = l_e.faceB().idx.value;
            if (!face_components.equivalent(id_A, id_B)) {
                --num_components;
                face_components.merge(id_A, id_B);
            }
        }
    }
    return num_components;
}

}
