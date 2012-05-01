// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2011 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

// $Id$

#include "Shape_impl.h"
#include "Course.h"

#include <Atlas/Message/Element.h>

#include <wfmath/line.h>
#include <wfmath/polygon.h>
#include <wfmath/stream.h>

using Atlas::Message::Element;
using Atlas::Message::MapType;

using WFMath::CoordType;
using WFMath::Point;
using WFMath::Polygon;

using WFMath::numeric_constants;

template<int dim> class LinearCourse : public Course<dim, WFMath::Line>
{
};

Shape::Shape()
{
}

//////////////////////////////// Ball /////////////////////////////////

template<>
Polygon<2> MathShape<WFMath::Ball, 2>::outline(CoordType precision) const
{
    Polygon<2> shape_outline;
    CoordType radius = m_shape.radius();
    CoordType segments = radius * numeric_constants<CoordType>::pi() * 2.f / precision;
    // FIXME lrint this properly
    int count = (int)std::ceil(segments);
    CoordType seg = numeric_constants<CoordType>::pi() * 2.f / count;
    for (int i = 0; i < count; ++i) {
        shape_outline.addCorner(i, Point<2>(radius * std::cos(seg * i),
                                            radius * std::sin(seg * i)));
    }
    return shape_outline;
}

template<>
const char * MathShape<WFMath::Ball, 2>::getType() const
{
    return "circle";
}

template<>
int MathShape<WFMath::Ball, 2>::fromAtlas(const Element & data)
{
    int ret = -1;
    try {
        if (data.isMap()) {
            m_shape.fromAtlas(data.Map());
            ret = 0;
        }
    }
    catch (Atlas::Message::WrongTypeException e) {
    }
    return ret;
}

template<>
void MathShape<WFMath::Ball, 2>::toAtlas(MapType & data) const
{
    Element e = m_shape.toAtlas();
    if (e.isMap()) {
        data = e.Map();
        data["type"] = getType();
    }
}

/////////////////////////////// Course ////////////////////////////////

template<>
const char * MathShape<LinearCourse, 2>::getType() const
{
    return "course";
}

////////////////////////////// AxisBox ////////////////////////////////

template<>
const char * MathShape<WFMath::AxisBox, 2>::getType() const
{
    return "box";
}

template<>
int MathShape<WFMath::AxisBox, 2>::fromAtlas(const Element & data)
{
    int ret = -1;
    try {
        if (data.isMap()) {
            const MapType & datamap = data.Map();
            MapType::const_iterator I = datamap.find("points");
            if (I != datamap.end()) {
                m_shape.fromAtlas(I->second);
                ret = 0;
            }
        } else {
            m_shape.fromAtlas(data.asList());
            ret = 0;
        }
    }
    catch (Atlas::Message::WrongTypeException e) {
    }
    return ret;
}

template<>
void MathShape<WFMath::AxisBox, 2>::toAtlas(MapType & data) const
{
    data["type"] = getType();
    data["points"] = m_shape.toAtlas();
}

//////////////////////////////// Line /////////////////////////////////

template<>
const char * MathShape<WFMath::Line, 2>::getType() const
{
    return "line";
}

template<>
void MathShape<WFMath::Line, 2>::scale(WFMath::CoordType factor)
{
    for (size_t i = 0; i < m_shape.numCorners(); ++i) {
        Point<2> corner = m_shape.getCorner(i);
        m_shape.moveCorner(i, Point<2>(corner.x() * factor,
                                       corner.y() * factor));
    }
}

/////////////////////////////// Point /////////////////////////////////

template<>
const char * MathShape<Point, 2>::getType() const
{
    return "point";
}

template<>
bool MathShape<Point, 2>::intersect(const Point<2> & p) const
{
    return WFMath::Equal(m_shape, p);
}

template<>
int MathShape<Point, 2>::fromAtlas(const Element & data)
{
    int ret = -1;
    try {
        if (data.isMap()) {
            const MapType & datamap = data.Map();
            MapType::const_iterator I = datamap.find("pos");
            if (I != datamap.end()) {
                m_shape.fromAtlas(I->second);
                ret = 0;
            }
        } else {
            m_shape.fromAtlas(data.asList());
            ret = 0;
        }
    }
    catch (Atlas::Message::WrongTypeException e) {
    }
    return ret;
}

template<>
void MathShape<Point, 2>::toAtlas(MapType & data) const
{
    Element e = m_shape.toAtlas();
    if (e.isList()) {
        data["pos"] = e.asList();
        data["type"] = getType();
    }
}

////////////////////////////// Polygon ////////////////////////////////

template<>
Polygon<2> MathShape<Polygon, 2>::outline(CoordType precision) const
{
    return m_shape;
}

template<>
const char * MathShape<Polygon, 2>::getType() const
{
    return "polygon";
}

template<>
WFMath::CoordType MathShape<Polygon, 2>::area() const
{
    WFMath::CoordType area = 0;

    size_t n = m_shape.numCorners();
    for (size_t i = 0; i < n; ++i) {
        Point<2> corner = m_shape.getCorner(i);
        Point<2> corner2 = m_shape.getCorner((i + 1) % n);
        area += corner.x() * corner2.y();
        area -= corner.y() * corner2.x();
    }

   return std::fabs(area / 2.f);
}

template<>
void MathShape<Polygon, 2>::scale(WFMath::CoordType factor)
{
    for (size_t i = 0; i < m_shape.numCorners(); ++i) {
        Point<2> corner = m_shape.getCorner(i);
        m_shape.moveCorner(i, Point<2>(corner.x() * factor,
                                       corner.y() * factor));
    }
}

/////////////////////////////// RotBox ////////////////////////////////

template<>
const char * MathShape<WFMath::RotBox, 2>::getType() const
{
    return "rotbox";
}

template<>
int MathShape<WFMath::RotBox, 2>::fromAtlas(const Element & data)
{
    int ret = -1;
    try {
        if (data.isMap()) {
            m_shape.fromAtlas(data.Map());
            ret = 0;
        }
    }
    catch (Atlas::Message::WrongTypeException e) {
    }
    return ret;
}

template<>
void MathShape<WFMath::RotBox, 2>::toAtlas(MapType & data) const
{
    Element e = m_shape.toAtlas();
    if (e.isMap()) {
        data = e.Map();
        data["type"] = getType();
    }
}

///////////////////////////////////////////////////////////////////////

Shape * Shape::newFromAtlas(const MapType & data)
{
    MapType::const_iterator I = data.find("type");
    if (I == data.end() || !I->second.isString()) {
        return 0;
    }
    const std::string & type = I->second.String();
    Shape * new_shape = 0;
    if (type == "polygon") {
        new_shape = new MathShape<Polygon>;
    } else if (type == "line") {
        new_shape = new MathShape<WFMath::Line>;
    } else if (type == "circle") {
        new_shape = new MathShape<WFMath::Ball>;
    } else if (type == "point") {
        new_shape = new MathShape<Point>;
    } else if (type == "rotbox") {
        new_shape = new MathShape<WFMath::RotBox>;
    } else if (type == "box") {
        new_shape = new MathShape<WFMath::AxisBox>;
    }
    if (new_shape != 0) {
        int res = new_shape->fromAtlas(data);
        if (res != 0) {
            delete new_shape;
            new_shape = 0;
        }
    }
    return new_shape;
}

template class MathShape<WFMath::AxisBox, 2>;
template class MathShape<WFMath::Ball, 2>;
template class MathShape<WFMath::Line, 2>;
template class MathShape<Point, 2>;
template class MathShape<Polygon, 2>;
template class MathShape<WFMath::RotBox, 2>;
template class MathShape<LinearCourse, 2>;
