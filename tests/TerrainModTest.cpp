// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2009 Alistair Riddoch
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


#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "rulesets/TerrainModTranslator.h"
#include "stubs/common/stubCustom.h"
#include "stubs/modules/stubLocation.h"

#include <wfmath/quaternion.h>

#include <cassert>

using Atlas::Message::Element;
using Atlas::Message::ListType;
using Atlas::Message::MapType;

static int test_reparse()
{
    // Call parseData with polygon shape and valid points
    {
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "polygon";
        shape_desc["points"] = ListType(3, ListType(2, 1.));
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";
        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        Mercator::TerrainMod * tm1 = titm->parseData(pos, orientation);
        assert(tm1 != 0);

        // Re-parse the same data. Should create new instance.
        Mercator::TerrainMod * tm2 = titm->parseData(pos, orientation);
        assert(tm2 != 0);
        assert(tm2 != tm1);

        // Change it to 2D ball shape. This requires a new mod.
        shape_desc["type"] = "ball";
        shape_desc["radius"] = 1.f;
        shape_desc["position"] = ListType(2, 1.);
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";
        titm = new TerrainModTranslator(mod);
        Mercator::TerrainMod * tm3 = titm->parseData(pos, orientation);
        assert(tm3 != 0);
        assert(tm3 != tm1);

        // Change it to an adjustmod. This requires a new mod
        mod["type"] = "adjustmod";
        titm = new TerrainModTranslator(mod);
        Mercator::TerrainMod * tm4 = titm->parseData(pos, orientation);
        assert(tm4 != 0);
        assert(tm4 != tm1);

        delete titm;
    }

    return 0;
}

int main()
{
    {
        TerrainModTranslator * titm = new TerrainModTranslator(Atlas::Message::MapType());
        delete titm;
    }

    // Call parsePosition with empty height data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z < 0);
    }

    // Call parsePosition with int height data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        data["height"] = 1;
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z > 0);
    }

    // Call parsePosition with float height data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        data["height"] = 1.;
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z > 0);
    }

    // Call parsePosition with bad (string) height data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        data["height"] = "1.";
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z < 0);
    }

    // Call parsePosition with int heightoffset data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        data["heightoffset"] = 2;
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z > 0);
    }

    // Call parsePosition with float heightoffset data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        data["heightoffset"] = 2.;
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z > 0);
    }

    // Call parsePosition with bad (string) heightoffset data
    {
        WFMath::Point<3> pos(0,0,-1);

        MapType data;
        data["heightoffset"] = "1.";
        float z = TerrainModTranslator::parsePosition(pos, data);
        assert(z < 0);
    }

    ////////////////////// Concrete classes ///////////////////////////

    {
        TerrainModTranslator * titm = new TerrainModTranslator(Atlas::Message::MapType());
        delete titm;
    }


    // Call parseData with empty map
    {
        TerrainModTranslator * titm = new TerrainModTranslator(MapType());
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(!ret);

        delete titm;
    }

    // Call parseData with unknown shape
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "unknown_shape";
        mod["shape"] = shape_desc;

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(!ret);


        delete titm;
    }

    // Call parseData with ball shape
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "ball";
        mod["shape"] = shape_desc;

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(!ret);

        delete titm;
    }

    // Call parseData with ball shape and valid ball params
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "ball";
        shape_desc["radius"] = 1.f;
        shape_desc["position"] = ListType(2, 1.);
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(ret);

        delete titm;
    }

    // Call parseData with ball shape and valid ball and orientation
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "ball";
        shape_desc["radius"] = 1.f;
        shape_desc["position"] = ListType(2, 1.);
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation(0,0,0,1);

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(ret);

        delete titm;
    }

    // Call parseData with polygon shape and valid polygon params
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "polygon";
        shape_desc["points"] = ListType(3, ListType(2, 1.));
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(ret);

        delete titm;
    }

    // Call parseData with rotbox shape and valid rotbox params
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "rotbox";
        shape_desc["point"] = ListType(2, 1.);
        shape_desc["size"] = ListType(2, 1.);
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(ret);

        delete titm;
    }

    // Call parseData with ball shape and invalid ball params
    {
        MapType mod;
        MapType shape_desc;
        shape_desc["type"] = "ball";
        shape_desc["radius"] = 1.f;
        shape_desc["position"] = ListType(3, "1");
        mod["shape"] = shape_desc;
        mod["type"] = "levelmod";

        TerrainModTranslator * titm = new TerrainModTranslator(mod);
        WFMath::Point<3> pos(0,0,-1);
        WFMath::Quaternion orientation;

        Mercator::TerrainMod* ret = titm->parseData(pos, orientation);
        assert(!ret);

        delete titm;
    }

    return test_reparse();
}

// stubs

#include "common/log.h"

void log(LogLevel lvl, const std::string & msg)
{
}
