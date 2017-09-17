// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2017 Erik Ogenvik
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

#include "TestBase.h"
#include "TestWorld.h"

#include "server/Ruleset.h"
#include "server/ServerRouting.h"

#include "rulesets/Entity.h"

#include "common/compose.hpp"
#include "common/debug.h"

#include <Atlas/Objects/Anonymous.h>
#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/SmartPtr.h>

#include <cassert>
#include <rulesets/PhysicalDomain.h>
#include <common/TypeNode.h>
#include <rulesets/ModeProperty.h>
#include <rulesets/TerrainProperty.h>
#include <Mercator/BasePoint.h>
#include <Mercator/Terrain.h>
#include <rulesets/PropelProperty.h>
#include <rulesets/AngularFactorProperty.h>
#include <chrono>
#include <rulesets/VisibilityProperty.h>

#include "stubs/common/stubLog.h"

using Atlas::Message::Element;
using Atlas::Message::ListType;
using Atlas::Message::MapType;
using Atlas::Objects::Root;
using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Entity::RootEntity;

using String::compose;


class PhysicalDomainIntegrationTest : public Cyphesis::TestBase
{
    protected:
        static long m_id_counter;

    public:
        PhysicalDomainIntegrationTest();

        static long newId();

        void setup();

        void teardown();

        void test_static_entities_no_move();

        void test_determinism();

        void test_visibilityPerformance();
};

long PhysicalDomainIntegrationTest::m_id_counter = 0L;

PhysicalDomainIntegrationTest::PhysicalDomainIntegrationTest()
{
    ADD_TEST(PhysicalDomainIntegrationTest::test_static_entities_no_move);
    ADD_TEST(PhysicalDomainIntegrationTest::test_determinism);
    ADD_TEST(PhysicalDomainIntegrationTest::test_visibilityPerformance);

}

long PhysicalDomainIntegrationTest::newId()
{
    return ++m_id_counter;
}

void PhysicalDomainIntegrationTest::setup()
{
    m_id_counter = 0;
}

void PhysicalDomainIntegrationTest::teardown()
{

}
void PhysicalDomainIntegrationTest::test_static_entities_no_move()
{

    double tickSize = 1.0 / 15.0;

    TypeNode* rockType = new TypeNode("rock");
    ModeProperty* modePlantedProperty = new ModeProperty();
    modePlantedProperty->set("planted");

    Entity* rootEntity = new Entity("0", newId());
    TerrainProperty* terrainProperty = new TerrainProperty();
    Mercator::Terrain& terrain = terrainProperty->getData();
    terrain.setBasePoint(0, 0, Mercator::BasePoint(40));
    terrain.setBasePoint(0, 1, Mercator::BasePoint(40));
    terrain.setBasePoint(1, 0, Mercator::BasePoint(10));
    terrain.setBasePoint(1, 1, Mercator::BasePoint(10));
    rootEntity->setProperty("terrain", terrainProperty);
    rootEntity->m_location.m_pos = WFMath::Point<3>::ZERO();
    rootEntity->m_location.setBBox(WFMath::AxisBox<3>(WFMath::Point<3>(0, 0, -64), WFMath::Point<3>(64, 64, 64)));
    PhysicalDomain* domain = new PhysicalDomain(*rootEntity);

    Property<double>* massProp = new Property<double>();
    massProp->data() = 100;

    std::vector<Entity*> entities;

    for (size_t i = 0; i < 60; ++i) {
        for (size_t j = 0; j < 60; ++j) {
            long id = newId();
            std::stringstream ss;
            ss << "planted" << id;
            Entity* entity = new Entity(ss.str(), id);
            entity->setProperty("mass", massProp);
            entity->setType(rockType);
            entity->setProperty(ModeProperty::property_name, modePlantedProperty);
            entity->m_location.m_pos = WFMath::Point<3>(i, j, i + j);
            entity->m_location.setBBox(WFMath::AxisBox<3>(WFMath::Point<3>(-0.25f, -0.25f, 0), WFMath::Point<3>(-0.25f, -0.25f, 0.5f)));
            domain->addEntity(*entity);
            entities.push_back(entity);
        }
    }

    OpVector res;

    //First tick is setup, so we'll exclude that from time measurement
    domain->tick(tickSize, res);
    auto start = std::chrono::high_resolution_clock::now();
    //Inject ticks for two seconds
    for (int i = 0; i < 30; ++i) {
        domain->tick(tickSize, res);
    }

    std::stringstream ss;
    long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
    ss << "Average tick duration: " << milliseconds / 30.0 << " ms";
    log(INFO, ss.str());
    ss = std::stringstream();
    ss << "Physics per second: " << (milliseconds / 2.0) / 10.0 << " %";
    log(INFO, ss.str());

}

void PhysicalDomainIntegrationTest::test_determinism()
{

    double tickSize = 1.0 / 15.0;

    TypeNode* rockType = new TypeNode("rock");

    Entity* rootEntity = new Entity("0", newId());
    TerrainProperty* terrainProperty = new TerrainProperty();
    Mercator::Terrain& terrain = terrainProperty->getData();
    terrain.setBasePoint(0, 0, Mercator::BasePoint(40));
    terrain.setBasePoint(0, 1, Mercator::BasePoint(40));
    terrain.setBasePoint(1, 0, Mercator::BasePoint(10));
    terrain.setBasePoint(1, 1, Mercator::BasePoint(10));
    rootEntity->setProperty("terrain", terrainProperty);
    rootEntity->m_location.m_pos = WFMath::Point<3>::ZERO();
    rootEntity->m_location.setBBox(WFMath::AxisBox<3>(WFMath::Point<3>(0, 0, -64), WFMath::Point<3>(64, 64, 64)));
    PhysicalDomain* domain = new PhysicalDomain(*rootEntity);

    Property<double>* massProp = new Property<double>();
    massProp->data() = 100;

    std::vector<Entity*> entities;

    for (size_t i = 0; i < 10; ++i) {
        for (size_t j = 0; j < 10; ++j) {
            long id = newId();
            std::stringstream ss;
            ss << "free" << id;
            Entity* freeEntity = new Entity(ss.str(), id);
            freeEntity->setProperty("mass", massProp);
            freeEntity->setType(rockType);
            freeEntity->m_location.m_pos = WFMath::Point<3>(i, j, i + j);
            freeEntity->m_location.setBBox(WFMath::AxisBox<3>(WFMath::Point<3>(-0.25f, -0.25f, 0), WFMath::Point<3>(-0.25f, -0.25f, 0.5f)));
            domain->addEntity(*freeEntity);
            entities.push_back(freeEntity);
        }
    }

    OpVector res;

    //First tick is setup, so we'll exclude that from time measurement
    domain->tick(tickSize, res);
    auto start = std::chrono::high_resolution_clock::now();
    //Inject ticks for two seconds
    for (int i = 0; i < 30; ++i) {
        domain->tick(tickSize, res);
    }
    std::stringstream ss;
    long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
    ss << "Average tick duration: " << milliseconds / 30.0 << " ms";
    log(INFO, ss.str());
    ss = std::stringstream();
    ss << "Physics per second: " << (milliseconds / 2.0) / 10.0 << " %";
    log(INFO, ss.str());
}

void PhysicalDomainIntegrationTest::test_visibilityPerformance()
{

    double tickSize = 1.0 / 15.0;

    TypeNode* rockType = new TypeNode("rock");
    TypeNode* humanType = new TypeNode("human");

    PropelProperty* propelProperty = new PropelProperty();
    ////Move diagonally up
    propelProperty->data() = WFMath::Vector<3>(5, 5, 0);

    Property<double>* massProp = new Property<double>();
    massProp->data() = 10000;

    Entity* rootEntity = new Entity("0", newId());
    rootEntity->m_location.m_pos = WFMath::Point<3>::ZERO();
    WFMath::AxisBox<3> aabb(WFMath::Point<3>(-512, -512, 0), WFMath::Point<3>(512, 512, 64));
    rootEntity->m_location.setBBox(aabb);
    PhysicalDomain* domain = new PhysicalDomain(*rootEntity);

    TestWorld testWorld(*rootEntity);

    ModeProperty* modePlantedProperty = new ModeProperty();
    modePlantedProperty->set("planted");

    std::vector<Entity*> entities;

    int counter = 0;

    auto size = aabb.highCorner() - aabb.lowCorner();

    for (float i = aabb.lowCorner().x(); i <= aabb.highCorner().x(); i = i + (size.x() / 100.0f)) {
        for (float j = aabb.lowCorner().y(); j <= aabb.highCorner().y(); j = j + (size.y() / 100.0f)) {
            counter++;
            long id = newId();
            std::stringstream ss;
            ss << "planted" << id;
            Entity* plantedEntity = new Entity(ss.str(), id);
            plantedEntity->setProperty(ModeProperty::property_name, modePlantedProperty);
            plantedEntity->setType(rockType);
            plantedEntity->m_location.m_pos = WFMath::Point<3>(i, j, 0);
            plantedEntity->m_location.setBBox(WFMath::AxisBox<3>(WFMath::Point<3>(-0.25f, -0.25f, 0), WFMath::Point<3>(-0.25f, -0.25f, .2f)));
            domain->addEntity(*plantedEntity);
            entities.push_back(plantedEntity);
        }
    }

    {
        std::stringstream ss;
        ss << "Added " << counter << " planted entities at " << (size.x() / 100.0) << " meter interval.";
        log(INFO, ss.str());
    }

    int numberOfObservers = 200;

    std::vector<Entity*> observers;
    for (int i = 0; i < numberOfObservers; ++i) {
        long id = newId();
        std::stringstream ss;
        ss << "observer" << id;
        Entity* observerEntity = new Entity(ss.str(), id);
        observers.push_back(observerEntity);
        observerEntity->m_location.setSolid(false);
        observerEntity->setType(humanType);
        observerEntity->m_location.m_pos = WFMath::Point<3>(aabb.lowCorner().x() + (i * 4), aabb.lowCorner().y(), 0);
        observerEntity->m_location.setBBox(WFMath::AxisBox<3>(WFMath::Point<3>(-0.1f, -0.1f, 0), WFMath::Point<3>(0.1, 0.1, 2)));
        observerEntity->setProperty(PropelProperty::property_name, propelProperty);
        observerEntity->setFlags(entity_perceptive);
        observerEntity->setProperty("mass", massProp);
        domain->addEntity(*observerEntity);
    }


    OpVector res;

    //First tick is setup, so we'll exclude that from time measurement
    domain->tick(2, res);
    {
        auto start = std::chrono::high_resolution_clock::now();
        //Inject ticks for 20 seconds
        for (int i = 0; i < 15 * 20; ++i) {
            domain->tick(tickSize, res);
        }
        std::stringstream ss;
        long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        ss << "Average tick duration with " << numberOfObservers << " moving observers: " << milliseconds / (15. * 20.0) << " ms";
        log(INFO, ss.str());
        ss = std::stringstream();
        ss << "Physics per second with " << numberOfObservers << " moving observers: " << (milliseconds / 20.0) / 10.0 << " %";
        log(INFO, ss.str());
    }
    //Now stop the observers from moving, and measure again
    for (Entity* observer : observers) {
        domain->applyTransform(*observer, WFMath::Quaternion(), WFMath::Point<3>(), WFMath::Vector<3>::ZERO());
    }
    domain->tick(10, res);
    {
        auto start = std::chrono::high_resolution_clock::now();
        //Inject ticks for 1 seconds
        for (int i = 0; i < 15; ++i) {
            domain->tick(tickSize, res);
        }
        std::stringstream ss;
        long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        ss << "Average tick duration without moving observer: " << milliseconds / 15. << " ms";
        log(INFO, ss.str());
        ss = std::stringstream();
        ss << "Physics per second without moving observer: " << (milliseconds / 1.0) / 10.0 << " %";
        log(INFO, ss.str());
    }
}

void TestWorld::message(const Operation& op, LocatedEntity& ent)
{
}

LocatedEntity* TestWorld::addNewEntity(const std::string&,
                                       const Atlas::Objects::Entity::RootEntity&)
{
    return 0;
}

int main()
{
    PhysicalDomainIntegrationTest t;

    return t.run();
}

