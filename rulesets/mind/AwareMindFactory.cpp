/*
 Copyright (C) 2015 erik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rulesets/mind/AwareMindFactory.h>
#include <rulesets/mind/AwareMind.h>


AwareMindFactory::AwareMindFactory()
: mSharedTerrain(new SharedTerrain()), mAwarenessStoreProvider(new AwarenessStoreProvider(*mSharedTerrain))
{

}

AwareMindFactory::~AwareMindFactory()
{
}

BaseMind * AwareMindFactory::newMind(const std::string & id, long intId) const
{
    return new AwareMind(id, intId, *mSharedTerrain, *mAwarenessStoreProvider);
}


