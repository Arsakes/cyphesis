// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2004 Alistair Riddoch
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


#ifndef RULESETS_TERRAIN_MOD_PROPERTY_H
#define RULESETS_TERRAIN_MOD_PROPERTY_H

#include "rulesets/TerrainEffectorProperty.h"

#include "physics/Vector3D.h"

namespace Mercator {
    class TerrainMod;
}

class TerrainProperty;
class TerrainModTranslator;

/// \brief Class to handle Entity terrain modifier property
/// \ingroup PropertyClasses
class TerrainModProperty : public TerrainEffectorProperty {
  public:
    static const std::string property_name;
    static const std::string property_atlastype;

    TerrainModProperty();
    ~TerrainModProperty();

    TerrainModProperty * copy() const;

    virtual void install(LocatedEntity *, const std::string &);
    virtual void remove(LocatedEntity *, const std::string &);
    virtual void apply(LocatedEntity *);

    virtual HandlerResult operation(LocatedEntity *,
                                    const Operation &,
                                    OpVector &);

    /// \brief Constructs a Mercator::TerrainMod from Atlas data
    Mercator::TerrainMod * parseModData(const WFMath::Point<3>& pos,
                                        const WFMath::Quaternion& orientation) const;

    /// \brief Changes a modifier's position
    void move(LocatedEntity*);

    /// \brief Removes the modifier from the terrain
    void remove(LocatedEntity*);

    /// \brief Retrieve a sub attribute of the property
    int getAttr(const std::string &,
                 Atlas::Message::Element &);
    /// \brief Modify a sub attribute of the property
    void setAttr(const std::string &,
                 const Atlas::Message::Element &);

    HandlerResult move_handler(LocatedEntity * e,
                               const Operation & op,
                               OpVector & res);
    HandlerResult delete_handler(LocatedEntity * e,
                                 const Operation & op,
                                 OpVector & res);
  protected:

    TerrainModTranslator* m_translator;

};


#endif // RULESETS_TERRAIN_MOD_PROPERTY_H
