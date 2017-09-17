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


#ifndef RULESETS_DOMAIN_H
#define RULESETS_DOMAIN_H

#include "physics/Vector3D.h"
#include "common/OperationRouter.h"

#include <wfmath/vector.h>

#include <string>
#include <list>

class LocatedEntity;

class Location;

/// \brief Base class for movement domains
///
/// The movement domain implements movement in the game world, including
/// visibility calculations, collision detection and physics.
/// Motion objects interact with the movement domain.
class Domain
{
    protected:

        /**
         * @brief The entity to which this domain belongs.
         */
        LocatedEntity& m_entity;

    public:

        Domain(LocatedEntity& entity);

        virtual ~Domain();

        virtual void tick(double t, OpVector& res) = 0;

        /**
         * @brief Checks if the observing Entity can see the observed entity.
         *
         * This is done by using both a distance check as well as an outfit and wielded check.
         *
         * @param observingEntity The observer entity.
         * @param observedEntity The entity being looked at.
         * @return True if the observer entity can see the observed entity.
         */
        virtual bool isEntityVisibleFor(const LocatedEntity& observingEntity, const LocatedEntity& observedEntity) const = 0;

        /**
         * Adds a child entity to this domain. The child entity is guaranteed to be a direct child of the entity to which the domain belongs.
         *
         * @param entity A child entity.
         */
        virtual void addEntity(LocatedEntity& entity) = 0;

        /**
         * Removes a child entity from this domain. The child entity is guaranteed to be a direct child of the entity to which the domain belongs, and to have addEntity(...) being called earlier.
         *
         * @param entity A child entity.
         */
        virtual void removeEntity(LocatedEntity& entity) = 0;

        /**
         * Fills the supplied list with all entities in the domain that the supplied entity can currently observe.
         * @param observingEntity The entity that is observing.
         * @param entityList A list of entities.
         */
        virtual void getVisibleEntitiesFor(const LocatedEntity& observingEntity, std::list<LocatedEntity*>& entityList) const = 0;

        /**
         * Fills the supplied list with all entities in the domain that are currently observing the supplied entity.
         * @param observedEntity The entity which is being observed.
         * @return A list of entities.
         */
        virtual std::list<LocatedEntity*> getObservingEntitiesFor(const LocatedEntity& observedEntity) const
        {
            return std::list<LocatedEntity*>();
        }

        /**
         * Applies transformations to a child entity in the domain.
         *
         * Note that different domains handle this differently. Some will ignore some or all of the transformations.
         *
         * @param entity The child entity.
         * @param orientation New orientation, applied if valid.
         * @param pos New position, applied if valid.
         * @param velocity New velocity, applied if valid.
         */
        virtual void applyTransform(LocatedEntity& entity, const WFMath::Quaternion& orientation, const WFMath::Point<3>& pos, const WFMath::Vector<3>& velocity)
        {

        }

        /**
         * Refreshes the terrain within the supplied areas.
         * @param areas A collection of areas describing the terrain changed.
         */
        virtual void refreshTerrain(const std::vector<WFMath::AxisBox<2>>& areas)
        {

        }

        /**
         * Updates perceptive state of a child entity.
         *
         * This is called mainly when an entity becomes perceptive.
         * @param entity A child entity of the entity the domain belongs to.
         */
        virtual void toggleChildPerception(LocatedEntity& entity)
        {

        }


};

#endif // RULESETS_DOMAIN_H
