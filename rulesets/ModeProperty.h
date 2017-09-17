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

#ifndef RULESETS_MODEPROPERTY_H_
#define RULESETS_MODEPROPERTY_H_

#include "common/Property.h"

class ModeProperty : public Property<std::string> {
    public:

        enum class Mode {
            /**
             * Planted entities are stuck to the terrain. Their z-position is determined by the terrain at their origo.
             * They can optionally also have an offset, specified in "planted-offset" or "planted-scaled-offset".
             * Planted entities are not affected by physics.
             */
            Planted,

            /**
             * Fixed entities are fixed in the world. They are not affected by terrain.
             * Fixed entities are not affected by physics.
             */
            Fixed,

            /**
             * Free entities are handled by the physics engine.
             */
            Free,

            /**
             * This mode is used when the mode string isn't recognized.
             */
            Unknown
        };

        static const std::string property_name;

        ModeProperty();
        virtual ~ModeProperty();
        virtual void apply(LocatedEntity *);
        virtual ModeProperty * copy() const;
        virtual void set(const Atlas::Message::Element & val);

        Mode getMode() const {
            return m_mode;
        }
    private:
        Mode m_mode;

};

#endif /* RULESETS_MODEPROPERTY_H_ */
