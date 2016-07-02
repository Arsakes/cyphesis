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

#ifndef RULESETS_PROPELPROPERTY_H_
#define RULESETS_PROPELPROPERTY_H_

#include "common/Property.h"

#include <wfmath/vector.h>

/**
 * \brief Handles a propel force from the entity itself.
 *
 * This is to be used for entities that move on their own. For example any character.
 *
 * \ingroup PropertyClasses
 */
class PropelProperty: public PropertyBase {
    public:
        static const std::string property_name;
        static const std::string property_atlastype;

        PropelProperty();
        virtual ~PropelProperty();
        const WFMath::Vector<3> & data() const { return mData; }
        WFMath::Vector<3> & data() { return mData; }

        virtual int get(Atlas::Message::Element & val) const;
        virtual void set(const Atlas::Message::Element & val);
        virtual void add(const std::string & key,
                         Atlas::Message::MapType & map) const;
        virtual void add(const std::string & key,
                         const Atlas::Objects::Entity::RootEntity & ent) const;

        virtual PropelProperty * copy() const;
    protected:
        WFMath::Vector<3> mData;
};

#endif /* RULESETS_PROPELPROPERTY_H_ */
