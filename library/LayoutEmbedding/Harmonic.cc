#include "Harmonic.hh"

#include <LayoutEmbedding/Assert.hh>
#include <LayoutEmbedding/ExactPredicates.h>

#include <Eigen/SparseLU>

namespace LayoutEmbedding
{

namespace
{

/// Angle at to-vertex between given and next halfedge
auto calc_sector_angle(
        const pm::vertex_attribute<tg::pos3>& _pos,
        const pm::halfedge_handle& _h)
{
    const auto v1 = _pos[_h.next().vertex_to()] - _pos[_h.vertex_to()];
    const auto v2 = _pos[_h.vertex_from()] - _pos[_h.vertex_to()];
    return tg::angle_between(v1, v2);
}

double mean_value_weight(
        const pm::vertex_attribute<tg::pos3>& _pos,
        const pm::halfedge_handle& _h)
{
    if (_h.edge().is_boundary())
        return 0.0;

    const auto angle_l = calc_sector_angle(_pos, _h.prev());
    const auto angle_r = calc_sector_angle(_pos, _h.opposite());
    const auto edge_length = pm::edge_length(_h, _pos);
    double w_ij = (tan(angle_l.radians() / 2.0) + tan(angle_r.radians() / 2.0)) / edge_length;

    if (w_ij <= 0.0)
        w_ij = 1e-5;

    return w_ij;
}

}

bool harmonic(
        const pm::vertex_attribute<tg::pos3>& _pos,
        const pm::vertex_attribute<bool>& _constrained,
        const Eigen::MatrixXd& _constraint_values,
        Eigen::MatrixXd& _res,
        const double _lambda_uniform)
{
    LE_ASSERT(_pos.mesh().is_compact());

    const int n = _pos.mesh().vertices().size();
    const int d = _constraint_values.cols();
    LE_ASSERT(_constraint_values.rows() == n);

    if (_lambda_uniform > 0.0)
        std::cout << "Trying harmonic parametrization with " << _lambda_uniform * 100.0 << "% uniform weights." << std::endl;

    // Set up Laplace matrix and rhs
    Eigen::MatrixXd rhs = Eigen::MatrixXd::Zero(n, d);
    std::vector<Eigen::Triplet<double>> triplets;
    for (auto v : _pos.mesh().vertices())
    {
        const int i = v.idx.value;

        if (_constrained[v])
        {
            triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));
            rhs.row(i) = _constraint_values.row(i);
        }
        else
        {
            LE_ASSERT(!v.is_boundary());

            for (auto h : v.outgoing_halfedges())
            {
                const int j = h.vertex_to().idx.value;
                const double w_ij = (1.0 - _lambda_uniform) * mean_value_weight(_pos, h) + _lambda_uniform * 1.0;
                triplets.push_back(Eigen::Triplet<double>(i, j, w_ij));
                triplets.push_back(Eigen::Triplet<double>(i, i, -w_ij));
            }
        }
    }

    Eigen::SparseMatrix<double> L(n, n);
    L.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success)
        return false;

    _res = solver.solve(rhs);
    if (solver.info() != Eigen::Success)
        return false;

    return true;;
}

bool harmonic(
        const pm::vertex_attribute<tg::pos3>& _pos,
        const pm::vertex_attribute<bool>& _constrained,
        const pm::vertex_attribute<tg::dpos2>& _constraint_values,
        pm::vertex_attribute<tg::dpos2>& _res,
        const double _lambda_uniform)
{
    const int n = _pos.mesh().vertices().size();
    const int d = 2;

    // Convert constraints
    Eigen::MatrixXd constraint_values = Eigen::MatrixXd::Zero(n, d);
    for (auto v : _pos.mesh().vertices())
        constraint_values.row(v.idx.value) = Eigen::Vector2d(_constraint_values[v].x, _constraint_values[v].y);

    // Compute
    Eigen::MatrixXd res_mat;
    if (!harmonic(_pos, _constrained, constraint_values, res_mat, _lambda_uniform))
        return false;

    // Convert result
    _res = _pos.mesh().vertices().make_attribute<tg::dpos2>();
    for (auto v : _pos.mesh().vertices())
        _res[v] = tg::dpos2(res_mat(v.idx.value, 0), res_mat(v.idx.value, 1));

    return true;
}

namespace
{

const double* ptr(
        const tg::dpos2& _p)
{
    return &_p.x;
}

}

bool injective(
        const Parametrization& _param)
{
    exactinit();

    for (auto f : _param.mesh().faces())
    {
        LE_ASSERT(f.vertices().size() == 3);
        auto it = f.vertices().begin();
        const auto a = _param[*it];
        ++it;
        const auto b = _param[*it];
        ++it;
        const auto c = _param[*it];

        if (orient2d(ptr(a), ptr(b), ptr(c)) <= 0.0)
            return false;
    }

    return true;
}

}
