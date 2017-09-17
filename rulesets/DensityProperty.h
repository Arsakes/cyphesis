// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2016 Erik Ogenvik
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

#ifndef RULESETS_DENSITY_PROPERTY_H
#define RULESETS_DENSITY_PROPERTY_H

#include "common/Property.h"

/**
 * Density property updates the mass automatically when the size of the entity changes.
 *
 * Density is expressed as kg/m3.
 */
class DensityProperty: public Property<double>
{
    public:

        static const std::string property_name;
        static const std::string property_atlastype;

        virtual void apply(LocatedEntity *);
        virtual DensityProperty * copy() const;

        void updateMass(LocatedEntity *entity) const;

};

#endif // RULESETS_DENSITY_PROPERTY_H
