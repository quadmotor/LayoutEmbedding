#include "VirtualPathConflictSentinel.hh"

#include <LayoutEmbedding/Assert.hh>
#include <LayoutEmbedding/Connectivity.hh>

#include <LayoutEmbedding/Visualization/Visualization.hh>
#include <glow-extras/viewer/view.hh>

namespace LayoutEmbedding {

VirtualPathConflictSentinel::VirtualPathConflictSentinel(const Embedding& _em) :
    em(_em),
    v_label(em.target_mesh()),
    e_label(em.target_mesh()),
    f_label(em.target_mesh()),
    l_port(em.layout_mesh()),
    t_port(em.target_mesh())
{
}

void VirtualPathConflictSentinel::insert(const pm::vertex_handle& _v, const VirtualPathConflictSentinel::Label& _l)
{
    for (const auto& prev_l : v_label[_v]) {
        mark_conflicting(_l, prev_l);
    }
    v_label[_v].insert(_l);
}

void VirtualPathConflictSentinel::insert(const pm::edge_handle& _e, const VirtualPathConflictSentinel::Label& _l)
{
    for (const auto& prev_l : e_label[_e]) {
        mark_conflicting(_l, prev_l);
    }
    e_label[_e].insert(_l);
}

void VirtualPathConflictSentinel::insert(const pm::face_handle& _f, const VirtualPathConflictSentinel::Label& _l)
{
    for (const auto& prev_l : f_label[_f]) {
        mark_conflicting(_l, prev_l);
    }
    f_label[_f].insert(_l);
}

void VirtualPathConflictSentinel::insert_virtual_vertex(const VirtualVertex& _vv, const VirtualPathConflictSentinel::Label& _l)
{
    if (is_real_vertex(_vv)) {
        insert(real_vertex(_vv), _l);
    }
    else {
        insert(real_edge(_vv), _l);
    }
}

void VirtualPathConflictSentinel::insert_segment(const VirtualVertex& _vv0, const VirtualVertex& _vv1, const VirtualPathConflictSentinel::Label& _l)
{
    if (is_real_vertex(_vv0)) {
        if (is_real_vertex(_vv1)) {
            // (V,V) case
            const auto& v0 = real_vertex(_vv0);
            const auto& v1 = real_vertex(_vv1);
            const auto& he = pm::halfedge_from_to(v0, v1);
            LE_ASSERT(he.is_valid());
            const auto& e = he.edge();
            insert(e, _l);
        }
        else {
            // (V,E) case
            const auto& v = real_vertex(_vv0);
            const auto& e = real_edge(_vv1);
            const auto& f = triangle_with_edge_and_opposite_vertex(e, v);
            LE_ASSERT(f.is_valid());
            insert(f, _l);
        }
    }
    else {
        if (is_real_vertex(_vv1)) {
            // (E,V) case
            const auto& e = real_edge(_vv0);
            const auto& v = real_vertex(_vv1);
            const auto& f = triangle_with_edge_and_opposite_vertex(e, v);
            LE_ASSERT(f.is_valid());
            insert(f, _l);
        }
        else {
            // (E,E) case
            const auto& e0 = real_edge(_vv0);
            const auto& e1 = real_edge(_vv1);
            const auto& f = common_face(e0, e1);
            LE_ASSERT(f.is_valid());
            insert(f, _l);
        }
    }
}

void VirtualPathConflictSentinel::insert_path(const VirtualPath& _path, const VirtualPathConflictSentinel::Label& _l)
{
    LE_ASSERT(_path.size() >= 2);

    // Note: We deliberately skip the first and last element
    for (int i = 1; i < _path.size() - 1; ++i) {
        insert_virtual_vertex(_path[i], _l);
    }

    // Path segments ("virtual edges")
    for (int i = 0; i < _path.size() - 1; ++i) {
        insert_segment(_path[i], _path[i+1], _l);
    }

    // Additionally remember the directions (ports) through wich the path leaves / enters its endpoints.
    LE_ASSERT(is_real_vertex(_path[0]));
    LE_ASSERT(is_real_vertex(_path[_path.size()-1]));

    // Warning: Here we rely on the assumption that for each edge l_e, the corresponding path was traced
    // using find_shortest_path(l_e.halfedgeA());
    const pm::edge_handle l_e = em.layout_mesh().edges()[_l];
    LE_ASSERT(em.matching_target_vertex(l_e.halfedgeA().vertex_from()) == real_vertex(_path[0]));

    const VirtualPort port_A(real_vertex(_path[0]), _path[1]);
    const VirtualPort port_B(real_vertex(_path[_path.size()-1]), _path[_path.size()-2]);
    // Links from layout to target
    l_port[l_e.halfedgeA()] = port_A;
    l_port[l_e.halfedgeB()] = port_B;
    // Links from target to layout
    t_port[_path[1]].insert(l_e);
    t_port[_path[_path.size()-2]].insert(l_e);
}

void VirtualPathConflictSentinel::mark_conflicting(const VirtualPathConflictSentinel::Label& _a, const VirtualPathConflictSentinel::Label& _b)
{
    if (_a == _b) {
        return;
    }

    global_conflicts.insert(_a);
    global_conflicts.insert(_b);

    const Conflict sorted = std::minmax(_a, _b);
    global_conflict_relation.insert(sorted);
}

void VirtualPathConflictSentinel::check_path_ordering()
{
    const auto reachable_by_sweep_ccw_in_sector = [&](VirtualPort& _start, const VirtualPort& _end) {
        LE_ASSERT(_start.from == _end.from);
        if (_start == _end) {
            // If both ports are identical then the corresponding paths are conflicting.
            return false;
        }
        while (_start != _end) {
            _start = _start.rotated_ccw();
            if (is_real_vertex(_start.to)) {
                auto t_he = pm::halfedge_from_to(_start.from, real_vertex(_start.to));
                if (em.is_blocked(t_he.edge())) {
                    // Reached a sector boundary!
                    return false;
                }
            }
        }
        return true;
    };

    const auto mark_and_sweep_cw_in_sector = [&](VirtualPort& _start, const VirtualPort& _end, const Label& _l) {
        LE_ASSERT(_start.from == _end.from);

        // Mark all encountered labels as conflicting.
        for (const auto& l : t_port[_start.to]) {
            mark_conflicting(_l, l);
        }

        while (_start != _end) {
            _start = _start.rotated_cw();
            if (is_real_vertex(_start.to)) {
                auto t_he = pm::halfedge_from_to(_start.from, real_vertex(_start.to));
                if (em.is_blocked(t_he.edge())) {
                    // Reached a sector boundary before reaching _end. This is a bug!
                    LE_ASSERT(false);
                }
            }
            // Mark all encountered labels as conflicting.
            for (const auto& l : t_port[_start.to]) {
                mark_conflicting(_l, l);
            }
        }
    };

    for (const auto l_v : em.layout_mesh().vertices()) {
        bool vertex_has_sectors = false;
        for (const auto l_sector_boundary_he : l_v.outgoing_halfedges()) {
            if (em.is_embedded(l_sector_boundary_he)) {
                vertex_has_sectors = true;

                // If a halfedge is embedded, the part following it is one "sector".
                // We now visit all layout halfedges in this sector (l_he_in_sector) in a CCW order
                // (i.e. until we reach another "embedded" layout halfedge).
                auto l_current_he_in_sector = rotated_ccw(l_sector_boundary_he);

                // Meanwhile, we keep track of the embedded directions (represented by current_port) of the corresponding layout halfedges in the sector.
                // We will check whether this direction also keeps "increasing" monotonically (a.k.a. rotating CCW) as we advance.
                // If current_port "decreases" (rotates CW), this introduces potential conflicts.
                // In that case, all ports that are swept over by a decreasing update of current_port will be marked as conflicting.
                VirtualPort current_port = l_port[l_current_he_in_sector];

                // Visit all layout halfedges in the current sector.
                while (true) {
                    if (em.is_embedded(l_current_he_in_sector)) {
                        break; // Reached end of sector
                    }
                    LE_ASSERT(current_port.is_valid());

                    auto l_next_he_in_sector = rotated_ccw(l_current_he_in_sector);
                    if (em.is_embedded(l_next_he_in_sector)) {
                        break; // Reached end of sector
                    }
                    auto next_port = l_port[l_next_he_in_sector];
                    LE_ASSERT(next_port.is_valid());

                    // Try to reach next_port from current_port using CCW rotations (without leaving the sector).
                    if (reachable_by_sweep_ccw_in_sector(current_port, next_port)) {
                        // All good, proceed.
                    }
                    else {
                        // Reach next_port from current_port using CW rotations (without leaving the sector).
                        // All candidate edges that are visited during the CW sweep will be marked "conflicting".
                        const Label& l = l_current_he_in_sector.edge().idx;
                        mark_and_sweep_cw_in_sector(current_port, next_port, l);
                    }

                    // Advance to the next halfedge in this sector.
                    l_current_he_in_sector = l_next_he_in_sector;
                    current_port = next_port;
                }
            }
        }

        if (!vertex_has_sectors) {
            // No sectors around the vertex (yet).
            // The best we can do is verify that the cyclic order of embedded edges matches that of the layout.
            // If so, there are no (additional) conflicts. Otherwise, we consider all edges around this vertex as conflicting.
            // TODO: Is it really necessary to mark all outgoing paths as conflicting in that case?
            // Can we do something that causes less branching?

            const auto l_he_start = l_v.any_outgoing_halfedge();
            auto l_he = l_he_start;
            LE_ASSERT(!em.is_embedded(l_he));

            const VirtualPort t_port_start = l_port[l_he_start];
            VirtualPort t_port = l_port[l_he];

            bool cyclic_conflict = false;
            do {
                const auto l_he_next = rotated_ccw(l_he);
                LE_ASSERT(!em.is_embedded(l_he));

                const auto t_port_next = l_port[l_he];

                // Try to reach t_port_next from t_port via CCW rotations without crossing t_port_start.
                while (t_port != t_port_next) {
                    t_port = t_port.rotated_ccw();

                    if (t_port == t_port_start) {
                        // We have cycled once around the embedded vertex before completing a cycle in the layout.
                        cyclic_conflict = true;
                        break;
                    }
                }

                l_he = l_he_next;
            }
            while (l_he != l_he_start && !cyclic_conflict);

            if (cyclic_conflict) {
                // Mark all surrounding edges as "conflicting"
                for (const auto l_e_A : l_v.edges()) {
                    for (const auto l_e_B : l_v.edges()) {
                        mark_conflicting(l_e_A, l_e_B);
                    }
                }
            }
        }
    }
}

}
