// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Edges.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <TopoDS_Shape.hxx>

#include <memory>
#include <optional>
#include <vector>

// Measuring (slice 25): what a face, an edge or a point is, and how far apart (and at
// what angle) two of them are. Everything in plate coordinates, millimetres.
namespace forge::measure {

// Something picked to measure: a face or an edge of a solid shape, or a point.
struct Target {
    enum class Kind { Point, Edge, Face };
    Kind kind = Kind::Point;
    TopoDS_Shape shape;   // the face or edge (null for a point)
    Vec3 point{0, 0, 0};  // where it was clicked (a point: the point itself)
};

// A solid shape ready for picking its faces and edges.
class Shape {
public:
    explicit Shape(const TopoDS_Shape& placed);
    ~Shape();
    Shape(const Shape&) = delete;
    Shape& operator=(const Shape&) = delete;

    // Every edge between two faces (no seams), as lines for drawing and picking.
    const std::vector<std::vector<Vec3>>& edgeLines() const { return m_lines; }
    // Their ends (corners), for snapping points to.
    const std::vector<Vec3>& corners() const { return m_corners; }
    std::optional<edges::FaceHit> faceUnder(const Vec3& origin, const Vec3& direction) const;
    Target edge(int index, const Vec3& clicked) const;
    Target face(int index, const Vec3& clicked) const;
    // Triangles over a face, for showing it picked.
    Mesh faceMesh(int index) const;
    double size() const { return m_size; }  // the shape's diagonal

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::vector<std::vector<Vec3>> m_lines;
    std::vector<Vec3> m_corners;
    double m_size = 1.0;
};

// What one thing is.
struct About {
    enum class Kind { Point, Line, Circle, Arc, Curve, Flat, Cylinder, Cone, Sphere, Torus, Surface };
    Kind kind = Kind::Point;
    Vec3 point{0, 0, 0};      // a point: where
    double length = 0.0;      // an edge: along it
    double area = 0.0;        // a face
    double radius = 0.0;      // circles, arcs, cylinders, spheres (tori: the tube's)
    double bigRadius = 0.0;   // tori: of the ring
    double halfAngle = 0.0;   // cones, degrees
    bool hasCentre = false;   // circle, arc, sphere: a centre point; cylinder, cone: an axis line
    Vec3 centre{0, 0, 0};
    bool hasDirection = false;  // lines: along; flat faces: facing; cylinders and cones: their axis
    Vec3 direction{0, 0, 1};
};
About describe(const Target& t);

// How two things stand to each other.
struct Between {
    double distance = 0.0;  // the nearest they come (0 if they touch)
    Vec3 from{0, 0, 0}, to{0, 0, 0};  // the nearest points, on the first and the second
    std::optional<double> angle;      // degrees, 0..90, if both have a direction
    bool parallel = false;            // both directions the same (or opposite)
    std::optional<double> apart;      // parallel flat faces or lines: how far apart they lie
    std::optional<double> centres;    // both have centres or axes: centre to centre (axis to axis)
    // A flat face and something with a centre (a circle, a ball) or an axis along the face
    // (a hole, a peg): from the centre or axis straight to the face.
    std::optional<double> centreToFace;
    Vec3 centreFrom{0, 0, 0}, centreTo{0, 0, 0};  // the line drawn for centres or centreToFace
};
Between between(const Target& a, const Target& b);

}  // namespace forge::measure
