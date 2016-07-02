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

#include "TransformsProperty.h"
#include "LocatedEntity.h"
#include "common/BaseWorld.h"

#include <wfmath/atlasconv.h>

#include <Atlas/Objects/SmartPtr.h>
#include <Atlas/Objects/RootEntity.h>

using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Objects::Entity::RootEntity;

const std::string TransformsProperty::property_name = "transforms";

TransformsProperty::TransformsProperty() {
}

WFMath::Vector<3>& TransformsProperty::getTranslate() {
    return mTransform.translate;
}

const WFMath::Vector<3>& TransformsProperty::getTranslate() const {
    return mTransform.translate;
}

WFMath::Quaternion& TransformsProperty::getRotate() {
    return mTransform.rotate;
}
const WFMath::Quaternion& TransformsProperty::getRotate() const {
    return mTransform.rotate;
}

WFMath::Vector<3>& TransformsProperty::getTranslateScaled() {
    return mTransform.translateScaled;
}

const WFMath::Vector<3>& TransformsProperty::getTranslateScaled() const {
    return mTransform.translateScaled;
}

void TransformsProperty::apply(LocatedEntity *entity) {
    WFMath::Quaternion rotation = mTransform.rotate;
    WFMath::Point<3> translation = WFMath::Point<3>(mTransform.translate);

    if (mTransform.translateScaled.isValid()
            && entity->m_location.bBox().isValid()) {
        auto size = (entity->m_location.bBox().highCorner()
                - entity->m_location.bBox().lowCorner());
        translation += WFMath::Vector<3>(
                mTransform.translateScaled.x() * size.x(),
                mTransform.translateScaled.y() * size.y(),
                mTransform.translateScaled.z() * size.z());
    }

    for (auto& entry : mExternal) {
        if (entry.second.rotate.isValid()) {
            Quaternion localRotation(entry.second.rotate);
            //normalize to avoid drift
            localRotation.normalize();
            rotation = localRotation * rotation;
        }
        if (entry.second.translate.isValid()) {
            translation += entry.second.translate;
        }
        if (entry.second.translateScaled.isValid()
                && entity->m_location.bBox().isValid()) {

            auto size = (entity->m_location.bBox().highCorner()
                    - entity->m_location.bBox().lowCorner());
            translation += WFMath::Vector<3>(
                    entry.second.translateScaled.x() * size.x(),
                    entry.second.translateScaled.y() * size.y(),
                    entry.second.translateScaled.z() * size.z());
        }
    }

    bool hadChange = false;
    if (entity->m_location.m_pos != translation) {
        entity->m_location.m_pos = translation;
        hadChange = true;
    }
    if (entity->m_location.m_orientation != rotation) {
        entity->m_location.m_orientation = rotation;
        hadChange = true;
    }

    if (hadChange) {
        entity->resetFlags(entity_pos_clean | entity_orient_clean);
        entity->setFlags(entity_dirty_location);
        //entity->m_location.update(BaseWorld::instance().getTime());
    }
}

int TransformsProperty::get(Element & val) const {
    std::map<std::string, Element> valMap;
    mTransform.get(valMap);

    if (!mExternal.empty()) {
        std::map<std::string, Element> externalMap;

        for (auto& entry : mExternal) {
            std::map<std::string, Element> entryMap;

            if (entry.second.rotate.isValid()) {
                entryMap["rotate"] = entry.second.rotate.toAtlas();
            }
            if (entry.second.translate.isValid()) {
                entryMap["translate"] = entry.second.translate.toAtlas();
            }
            if (entry.second.translateScaled.isValid()) {
                entryMap["translate-scaled"] = entry.second.translate.toAtlas();
            }
            externalMap.insert(std::make_pair(entry.first, entryMap));
        }
        valMap["external"] = externalMap;
    }
    val = valMap;
    return 0;
}

void TransformsProperty::set(const Element & val) {

    mTransform.set(val);
    if (val.isMap()) {
        auto& valMap = val.Map();

        auto I = valMap.find("external");
        if (I != valMap.end() && I->second.isMap()) {
            const auto& externalMap = I->second.Map();

            for (auto& entry : externalMap) {
                Transform transform;
                if (entry.second.isMap()) {

                    auto& entryMap = entry.second.Map();
                    auto I = entryMap.find("rotate");
                    if (I != entryMap.end()) {
                        transform.rotate = WFMath::Quaternion(I->second);
                    }
                    I = entryMap.find("translate");
                    if (I != entryMap.end()) {
                        transform.translate = WFMath::Vector<3>(I->second);
                    }
                    I = entryMap.find("translate-scaled");
                    if (I != entryMap.end()) {
                        transform.translateScaled = WFMath::Vector<3>(
                                I->second);
                    }
                }
            }
        }
    }
}

TransformsProperty * TransformsProperty::copy() const {
    return new TransformsProperty(*this);
}

std::map<std::string, Transform>& TransformsProperty::external() {
    return mExternal;
}

const std::map<std::string, Transform>& TransformsProperty::external() const {
    return mExternal;
}

