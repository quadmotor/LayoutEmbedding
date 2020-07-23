#include "Connectivity.hh"

#include <LayoutEmbedding/Assert.hh>

namespace LayoutEmbedding {

bool adjacent(const pm::vertex_handle& _v0, const pm::vertex_handle& _v1)
{
    LE_ASSERT(_v0.mesh == _v1.mesh);
    const auto he = pm::halfedge_from_to(_v0, _v1);
    return he.is_valid();
}

bool incident(const pm::vertex_handle& _v, const pm::edge_handle& _e)
{
    LE_ASSERT(_v.mesh == _e.mesh);
    return (_e.vertexA() == _v) || (_e.vertexB() == _v);
}

bool incident(const pm::edge_handle& _e, const pm::vertex_handle& _v)
{
    return incident(_v, _e);
}

pm::vertex_handle opposite_vertex(const pm::halfedge_handle& _he)
{
    if (_he.is_boundary()) {
        return pm::vertex_handle::invalid;
    }
    else {
        return _he.next().vertex_to();
    }
}

}
