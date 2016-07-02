/*
 Copyright (C) 2014 Erik Ogenvik

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

 Some portions of this file include code taken from the OgreCrowd project, which has the copyrights and license as described below.
 These portions are the findPath() and processTiles() methods.

 OgreCrowd
 ---------

 Copyright (c) 2012 Jonas Hauquier

 Additional contributions by:

 - mkultra333
 - Paul Wilson

 Sincere thanks and to:

 - Mikko Mononen (developer of Recast navigation libraries)

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.


 */

#include "Awareness.h"
#include "AwarenessUtils.h"

#include "IHeightProvider.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourNavMeshBuilder.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "DetourCommon.h"
#include "DetourObstacleAvoidance.h"

#include "common/debug.h"

#include "rulesets/MemEntity.h"

#include <wfmath/wfmath.h>

#include <Atlas/Message/Element.h>

#include <sigc++/bind.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include <cmath>
#include <vector>
#include <cstring>
#include <queue>

static const bool debug_flag = false;

#define MAX_PATHPOLY      256 // max number of polygons in a path
#define MAX_PATHVERT      512 // most verts in a path
#define MAX_OBSTACLES_CIRCLES 4 // max number of circle obstacles to consider when doing avoidance

// This value specifies how many layers (or "floors") each navmesh tile is expected to have.
static const int EXPECTED_LAYERS_PER_TILE = 1;

using namespace boost::multi_index;

/**
 * @brief A Most Recently Used list implemented using boost::multi_index.
 *
 */
template<typename TItem>
class MRUList
{
    public:

        void insert(const TItem& item)
        {
            auto p = mItems.push_front(item);

            if (!p.second) {
                mItems.relocate(mItems.begin(), p.first);
            }
        }

        TItem pop_back()
        {
            TItem back = mItems.back();
            mItems.pop_back();
            return back;
        }

        std::size_t size() const
        {
            return mItems.size();
        }

    private:
        multi_index_container<TItem, indexed_by<sequenced<>, hashed_unique<identity<TItem> > > > mItems;

};

struct InputGeometry
{
        std::vector<float> verts;
        std::vector<int> tris;
        std::vector<WFMath::RotBox<2>> entityAreas;
};

class AwarenessContext: public rcContext
{
    protected:
        virtual void doLog(const rcLogCategory category, const char* msg, const int len)
        {
            if (category == RC_LOG_PROGRESS) {
                ::log(INFO, String::compose("Recast: %1", msg));
            } else if (category == RC_LOG_WARNING) {
                ::log(WARNING, String::compose("Recast: %1", msg));
            } else {
                ::log(ERROR, String::compose("Recast: %1", msg));
            }
        }

};

Awareness::Awareness(const LocatedEntity& domainEntity, float agentRadius, float agentHeight, IHeightProvider& heightProvider, const WFMath::AxisBox<3>& extent, int tileSize) :
        mHeightProvider(heightProvider), mDomainEntity(domainEntity), mTalloc(nullptr), mTcomp(nullptr), mTmproc(nullptr), mAgentRadius(agentRadius), mBaseTileAmount(128), mDesiredTilesAmount(128), mCtx(
                new AwarenessContext()), mTileCache(nullptr), mNavMesh(nullptr), mNavQuery(dtAllocNavMeshQuery()), mFilter(nullptr), mActiveTileList(nullptr), mObserverCount(0)
{
    debug_print("Creating awareness with extent " << extent << " and agent radius " << agentRadius);
    try {
        mActiveTileList = new MRUList<std::pair<int, int>>();

        mTalloc = new LinearAllocator(128000);
        mTcomp = new FastLZCompressor;
        mTmproc = new MeshProcess;

        // Setup the default query filter
        mFilter = new dtQueryFilter();
        mFilter->setIncludeFlags(0xFFFF); // Include all
        mFilter->setExcludeFlags(0); // Exclude none
        // Area flags for polys to consider in search, and their cost
        mFilter->setAreaCost(POLYAREA_GROUND, 1.0f);

        const WFMath::Point<3>& lower = extent.lowCorner();
        const WFMath::Point<3>& upper = extent.highCorner();

        //Recast uses y for the vertical axis
        mCfg.bmin[0] = lower.x();
        mCfg.bmin[2] = lower.y();
        mCfg.bmin[1] = std::min(-500.f, lower.z());
        mCfg.bmax[0] = upper.x();
        mCfg.bmax[2] = upper.y();
        mCfg.bmax[1] = std::max(500.f, upper.z());

        int gw = 0, gh = 0;
        float cellsize = mAgentRadius / 2.0f; //Should be enough for outdoors; indoors we might want r / 3.0 instead
        rcCalcGridSize(mCfg.bmin, mCfg.bmax, cellsize, &gw, &gh);
        const int tilewidth = (gw + tileSize - 1) / tileSize;
        const int tileheight = (gh + tileSize - 1) / tileSize;

        // Max tiles and max polys affect how the tile IDs are caculated.
        // There are 22 bits available for identifying a tile and a polygon.
        int tileBits = rcMin((int)dtIlog2(dtNextPow2(tilewidth * tileheight * EXPECTED_LAYERS_PER_TILE)), 14);
        if (tileBits > 14)
            tileBits = 14;
        int polyBits = 22 - tileBits;
        unsigned int maxTiles = 1 << tileBits;
        unsigned int maxPolysPerTile = 1 << polyBits;

        //For an explanation of these values see http://digestingduck.blogspot.se/2009/08/recast-settings-uncovered.html

        mCfg.cs = cellsize;
        mCfg.ch = mCfg.cs / 2.0f; //Height of one voxel; should really only come into play when doing 3d traversal
        //	m_cfg.ch = std::max(upper.z() - lower.z(), 100.0f); //For 2d traversal make the voxel size as large as possible
        mCfg.walkableHeight = std::ceil(agentHeight / mCfg.ch); //This is in voxels
        mCfg.walkableClimb = 100; //TODO: implement proper system for limiting climbing; for now just use a large voxel number
        mCfg.walkableRadius = std::ceil(mAgentRadius / mCfg.cs);
        mCfg.walkableSlopeAngle = 70; //TODO: implement proper system for limiting climbing; for now just use 70 degrees

        mCfg.maxEdgeLen = mCfg.walkableRadius * 8.0f;
        mCfg.maxSimplificationError = 1.3f;
        mCfg.minRegionArea = (int)rcSqr(8);
        mCfg.mergeRegionArea = (int)rcSqr(20);

        mCfg.tileSize = tileSize;
        mCfg.borderSize = mCfg.walkableRadius + 3; // Reserve enough padding.
        mCfg.width = mCfg.tileSize + mCfg.borderSize * 2;
        mCfg.height = mCfg.tileSize + mCfg.borderSize * 2;
        //	m_cfg.detailSampleDist = m_detailSampleDist < 0.9f ? 0 : m_cfg.cs * m_detailSampleDist;
        //	m_cfg.detailSampleMaxError = m_cfg.m_cellHeight * m_detailSampleMaxError;

        // Tile cache params.
        dtTileCacheParams tcparams;
        memset(&tcparams, 0, sizeof(tcparams));
        rcVcopy(tcparams.orig, mCfg.bmin);
        tcparams.cs = mCfg.cs;
        tcparams.ch = mCfg.ch;
        tcparams.width = (int)mCfg.tileSize;
        tcparams.height = (int)mCfg.tileSize;
        tcparams.walkableHeight = agentHeight;
        tcparams.walkableRadius = mAgentRadius;
        tcparams.walkableClimb = mCfg.walkableClimb;
        //	tcparams.maxSimplificationError = m_edgeMaxError;
        tcparams.maxTiles = tilewidth * tileheight * EXPECTED_LAYERS_PER_TILE;
        tcparams.maxObstacles = 128;

        dtFreeTileCache(mTileCache);

        dtStatus status;

        mTileCache = dtAllocTileCache();
        if (!mTileCache) {
            throw std::runtime_error("buildTiledNavigation: Could not allocate tile cache.");
        }
        status = mTileCache->init(&tcparams, mTalloc, mTcomp, mTmproc);
        if (dtStatusFailed(status)) {
            throw std::runtime_error("buildTiledNavigation: Could not init tile cache.");
        }

        dtFreeNavMesh(mNavMesh);

        mNavMesh = dtAllocNavMesh();
        if (!mNavMesh) {
            throw std::runtime_error("buildTiledNavigation: Could not allocate navmesh.");
        }

        dtNavMeshParams params;
        memset(&params, 0, sizeof(params));
        rcVcopy(params.orig, mCfg.bmin);
        params.tileWidth = tileSize * cellsize;
        params.tileHeight = tileSize * cellsize;
        params.maxTiles = maxTiles;
        params.maxPolys = maxPolysPerTile;

        status = mNavMesh->init(&params);
        if (dtStatusFailed(status)) {
            throw std::runtime_error("buildTiledNavigation: Could not init navmesh.");
        }

        status = mNavQuery->init(mNavMesh, 2048);
        if (dtStatusFailed(status)) {
            throw std::runtime_error("buildTiledNavigation: Could not init Detour navmesh query");
        }

        mObstacleAvoidanceQuery = dtAllocObstacleAvoidanceQuery();
        mObstacleAvoidanceQuery->init(MAX_OBSTACLES_CIRCLES, 0);

        mObstacleAvoidanceParams = new dtObstacleAvoidanceParams;
        mObstacleAvoidanceParams->velBias = 0.4f;
        mObstacleAvoidanceParams->weightDesVel = 2.0f;
        mObstacleAvoidanceParams->weightCurVel = 0.75f;
        mObstacleAvoidanceParams->weightSide = 0.75f;
        mObstacleAvoidanceParams->weightToi = 2.5f;
        mObstacleAvoidanceParams->horizTime = 2.5f;
        mObstacleAvoidanceParams->gridSize = 33;
        mObstacleAvoidanceParams->adaptiveDivs = 7;
        mObstacleAvoidanceParams->adaptiveRings = 2;
        mObstacleAvoidanceParams->adaptiveDepth = 5;

    } catch (const std::exception& e) {
        delete mObstacleAvoidanceParams;
        dtFreeObstacleAvoidanceQuery(mObstacleAvoidanceQuery);

        dtFreeNavMesh(mNavMesh);
        dtFreeNavMeshQuery(mNavQuery);
        delete mFilter;

        dtFreeTileCache(mTileCache);

        delete mTmproc;
        delete mTcomp;
        delete mTalloc;

        delete mCtx;
        delete mActiveTileList;
        throw;
    }
}

Awareness::~Awareness()
{

    delete mObstacleAvoidanceParams;
    dtFreeObstacleAvoidanceQuery(mObstacleAvoidanceQuery);

    dtFreeNavMesh(mNavMesh);
    dtFreeNavMeshQuery(mNavQuery);
    delete mFilter;

    dtFreeTileCache(mTileCache);

    delete mTmproc;
    delete mTcomp;
    delete mTalloc;

    delete mCtx;
    delete mActiveTileList;
}

void Awareness::addObserver() {
    mObserverCount++;
    mDesiredTilesAmount = mBaseTileAmount + ((mObserverCount - 1) * (mBaseTileAmount * 0.4));
}

void Awareness::removeObserver() {
    mObserverCount--;
    if (mObserverCount == 0) {
        mDesiredTilesAmount = mBaseTileAmount;
    } else {
        mDesiredTilesAmount = mBaseTileAmount + ((mObserverCount - 1) * (mBaseTileAmount * 0.4));
    }
}

void Awareness::addEntity(const MemEntity& observer, const LocatedEntity& entity, bool isDynamic)
{
    auto I = mObservedEntities.find(entity.getIntId());
    if (I == mObservedEntities.end()) {
        std::unique_ptr<EntityEntry> entityEntry(new EntityEntry());
        entityEntry->entityId = entity.getIntId();
        entityEntry->numberOfObservers = 1;
//        entityEntry->location = entity.m_location;
        entityEntry->isIgnored = !entity.m_location.bBox().isValid();
        entityEntry->isMoving = isDynamic;
        entityEntry->isActorOwned = false;
        if (isDynamic) {
            mMovingEntities.insert(entityEntry.get());
        }
        I = mObservedEntities.insert(std::make_pair(entity.getIntId(), std::move(entityEntry))).first;
        debug_print("Creating new entry for " << entity.getId());
    } else {
        I->second->numberOfObservers++;
    }

    //Entity already exists; check if it's the same as the observer and marked it as owned.
    if (I->first == observer.getIntId()) {
        I->second->isActorOwned = true;
    }

    //Only process those entities that aren't owned by another actor, of if that's the case if the entity is ourself
    if (!I->second->isActorOwned || I->first == observer.getIntId()) {
        processEntityMovementChange(*I->second.get(), entity);
    }
}

void Awareness::removeEntity(const MemEntity& observer, const LocatedEntity& entity)
{
    auto I = mObservedEntities.find(entity.getIntId());
    if (I != mObservedEntities.end()) {
        debug_print("Removing entity " << entity.getId());
        //Decrease the number of observers, and delete entry if there's none left
        auto& entityEntry = I->second;
        if (entityEntry->numberOfObservers == 0) {
            log(WARNING, String::compose("Entity entry %1 has decreased number of observers to < 0. This indicates an error.", entity.getId()));
        }
        entityEntry->numberOfObservers--;
        if (entityEntry->numberOfObservers == 0) {
            if (entityEntry->isIgnored) {
                if (entityEntry->isMoving) {
                    mMovingEntities.erase(entityEntry.get());
                } else {
                    std::map<const EntityEntry*, WFMath::RotBox<2>> areas;

                    buildEntityAreas(*entityEntry.get(), areas);

                    for (auto& entry : areas) {
                        markTilesAsDirty(entry.second.boundingBox());
                    }
                    mEntityAreas.erase(entityEntry.get());
                }
                mObservedEntities.erase(I);
            }
        } else {
            if (observer.getIntId() == entity.getIntId()) {
                entityEntry->isActorOwned = false;
            }
        }
    }
}

void Awareness::updateEntityMovement(const MemEntity& observer, const LocatedEntity& entity)
{
    //This is called when either the position, orientation, velocity, location or size of the entity has been altered.
    auto I = mObservedEntities.find(entity.getIntId());
    if (I != mObservedEntities.end()) {
        EntityEntry* entityEntry = I->second.get();
        if (!entityEntry->isActorOwned || entityEntry->entityId == observer.getIntId()) {
            //If an entity was ignored previously because it didn't have a bbox, but now has, it shouldn't be ignored anymore.
            if (entityEntry->isIgnored && entity.m_location.bBox().isValid()) {
                debug_print("Stopped ignoring entity " << entity.getId());

                entityEntry->isIgnored = false;
            }
            processEntityMovementChange(*entityEntry, entity);
        }
    }
}

void Awareness::processEntityMovementChange(EntityEntry& entityEntry, const LocatedEntity& entity)
{
    //If entity already is moving we just need to update its location
    if (entityEntry.isMoving) {
        entityEntry.location = entity.m_location;
        //Otherwise check if the entity already isn't being ignored; if not we need to act as it means that
        //an entity which wasn't moving is now moving
    } else if (!entityEntry.isIgnored) {
        //Check if the bbox now is invalid
        if (!entity.m_location.bBox().isValid()) {
            debug_print("Ignoring entity " << entity.getId());
            entityEntry.location = entity.m_location;
            entityEntry.isIgnored = true;

            //We must now mark those areas that the entities used to touch as dirty, as well as remove the entity areas
            std::map<const EntityEntry*, WFMath::RotBox<2>> areas;

            buildEntityAreas(entityEntry, areas);

            for (auto& entry : areas) {
                markTilesAsDirty(entry.second.boundingBox());
            }
            mEntityAreas.erase(&entityEntry);

        } else {

            //Only update if there's a change
            if (((entityEntry.location.bBox().isValid() || entity.m_location.bBox().isValid()) && entityEntry.location.bBox() != entity.m_location.bBox())
                    || ((entityEntry.location.pos().isValid() || entity.m_location.pos().isValid()) && entityEntry.location.pos() != entity.m_location.pos())
                    || ((entityEntry.location.velocity().isValid() || entity.m_location.velocity().isValid()) && (entityEntry.location.velocity() != entity.m_location.velocity()))
                    || ((entityEntry.location.orientation().isValid() || entity.m_location.orientation().isValid()) && entityEntry.location.orientation() != entity.m_location.orientation())) {
                entityEntry.location = entity.m_location;

                debug_print("Updating entity location for entity " << entityEntry.entityId);

                //If an entity which previously didn't move start moving we need to move it to the "movable entities" collection.
                if (entity.m_location.m_velocity.isValid() && entity.m_location.m_velocity != WFMath::Vector<3>::ZERO()) {
                    debug_print("Entity is now moving.");
                    mMovingEntities.insert(&entityEntry);
                    entityEntry.isMoving = true;
                    auto existingI = mEntityAreas.find(&entityEntry);
                    if (existingI != mEntityAreas.end()) {
                        //The entity already was registered; mark those tiles where the entity previously were as dirty.
                        markTilesAsDirty(existingI->second.boundingBox());
                        mEntityAreas.erase(&entityEntry);
                    }
                } else {
                    std::map<const EntityEntry*, WFMath::RotBox<2>> areas;

                    buildEntityAreas(entityEntry, areas);

                    for (auto& entry : areas) {
                        markTilesAsDirty(entry.second.boundingBox());
                        auto existingI = mEntityAreas.find(entry.first);
                        if (existingI != mEntityAreas.end()) {
                            //The entity already was registered; mark both those tiles where the entity previously were as well as the new tiles as dirty.
                            markTilesAsDirty(existingI->second.boundingBox());
                            existingI->second = entry.second;
                        } else {
                            mEntityAreas.insert(entry);
                        }
                    }
                    debug_print(
                            "Entity affects " << areas.size() << " areas. Dirty unaware tiles: " << mDirtyUnwareTiles.size() << " Dirty aware tiles: " << mDirtyAwareTiles.size());
                }
            }
        }
    }
}

bool Awareness::avoidObstacles(int avatarEntityId, const WFMath::Point<2>& position, const WFMath::Vector<2>& desiredVelocity, WFMath::Vector<2>& newVelocity, double currentTimestamp) const
{
    struct EntityCollisionEntry
    {
            float distance;
            const EntityEntry* entity;
            WFMath::Point<2> viewPosition;
            WFMath::Ball<2> viewRadius;
    };

    auto comp = []( EntityCollisionEntry& a, EntityCollisionEntry& b ) {return a.distance < b.distance;};
    std::priority_queue<EntityCollisionEntry, std::vector<EntityCollisionEntry>, decltype( comp )> nearestEntities(comp);

    WFMath::Ball<2> playerRadius(position, 5);

    for (auto& entity : mMovingEntities) {

        //All of the entities have the same location as we have, so we don't need to resolve the position in the world.

        if (entity->entityId == avatarEntityId) {
            //Don't avoid ourselves.
            continue;
        }

        double time_diff = currentTimestamp - entity->location.timeStamp();

        // Update location
        Point3D pos = entity->location.pos();
        if (entity->location.velocity().isValid()) {
            pos += (entity->location.velocity() * time_diff);
        }

        if (!pos.isValid()) {
            continue;
        }

        WFMath::Point<2> entityView2dPos(pos.x(), pos.y());
        WFMath::Ball<2> entityViewRadius(entityView2dPos, entity->location.radius());

        if (WFMath::Intersect(playerRadius, entityViewRadius, false) || WFMath::Contains(playerRadius, entityViewRadius, false)) {
            nearestEntities.push(EntityCollisionEntry( { WFMath::Distance(position, entityView2dPos), entity, entityView2dPos, entityViewRadius }));
        }

    }

    if (!nearestEntities.empty()) {
        mObstacleAvoidanceQuery->reset();
        int i = 0;
        while (!nearestEntities.empty() && i < MAX_OBSTACLES_CIRCLES) {
            const EntityCollisionEntry& entry = nearestEntities.top();
            auto& entity = entry.entity;
            float pos[] { entry.viewPosition.x(), 0, entry.viewPosition.y() };
            float vel[] { entity->location.velocity().x(), 0, entity->location.velocity().y() };
            mObstacleAvoidanceQuery->addCircle(pos, entry.viewRadius.radius(), vel, vel);
            nearestEntities.pop();
            ++i;
        }

        float pos[] { position.x(), 0, position.y() };
        float vel[] { desiredVelocity.x(), 0, desiredVelocity.y() };
        float dvel[] { desiredVelocity.x(), 0, desiredVelocity.y() };
        float nvel[] { 0, 0, 0 };
        float desiredSpeed = desiredVelocity.mag();

        int samples = mObstacleAvoidanceQuery->sampleVelocityGrid(pos, mAgentRadius, desiredSpeed, vel, dvel, nvel, mObstacleAvoidanceParams, nullptr);
        if (samples > 0) {
            if (!WFMath::Equal(vel[0], nvel[0]) || !WFMath::Equal(vel[2], nvel[2])) {
                newVelocity.x() = nvel[0];
                newVelocity.y() = nvel[2];
                newVelocity.setValid(true);
                return true;
            }
        }

    }
    return false;

}

void Awareness::markTilesAsDirty(const WFMath::AxisBox<2>& area)
{
    int tileMinXIndex, tileMaxXIndex, tileMinYIndex, tileMaxYIndex;
    findAffectedTiles(area, tileMinXIndex, tileMaxXIndex, tileMinYIndex, tileMaxYIndex);
    markTilesAsDirty(tileMinXIndex, tileMaxXIndex, tileMinYIndex, tileMaxYIndex);
}

void Awareness::markTilesAsDirty(int tileMinXIndex, int tileMaxXIndex, int tileMinYIndex, int tileMaxYIndex)
{
    bool wereDirtyTiles = !mDirtyAwareTiles.empty();

    for (int tx = tileMinXIndex; tx <= tileMaxXIndex; ++tx) {
        for (int ty = tileMinYIndex; ty <= tileMaxYIndex; ++ty) {
            std::pair<int, int> index(tx, ty);
            if (mAwareTiles.find(index) != mAwareTiles.end()) {
                if (mDirtyAwareTiles.insert(index).second) {
                    mDirtyAwareOrderedTiles.push_back(index);
                }
            } else {
                mDirtyUnwareTiles.insert(index);
            }
        }
    }
    debug_print("Marking tiles as dirty. Aware: " << mDirtyAwareTiles.size() << " Unaware: " << mDirtyUnwareTiles.size());
    if (!wereDirtyTiles && !mDirtyAwareTiles.empty()) {
        EventTileDirty();
    }
}

size_t Awareness::rebuildDirtyTile()
{
    if (!mDirtyAwareTiles.empty()) {
        debug_print("Rebuilding aware tiles. Number of dirty aware tiles: " << mDirtyAwareTiles.size());
        const auto tileIndexI = mDirtyAwareOrderedTiles.begin();
        const auto& tileIndex = *tileIndexI;

        float tilesize = mCfg.tileSize * mCfg.cs;
        WFMath::AxisBox<2> adjustedArea(WFMath::Point<2>(mCfg.bmin[0] + (tileIndex.first * tilesize), mCfg.bmin[2] + (tileIndex.second * tilesize)),
                WFMath::Point<2>(mCfg.bmin[0] + ((tileIndex.first + 1) * tilesize), mCfg.bmin[2] + ((tileIndex.second + 1) * tilesize)));

        std::vector<WFMath::RotBox<2>> entityAreas;
        findEntityAreas(adjustedArea, entityAreas);

        rebuildTile(tileIndex.first, tileIndex.second, entityAreas);
        mDirtyAwareTiles.erase(tileIndex);
        mDirtyAwareOrderedTiles.erase(tileIndexI);
    }
    return mDirtyAwareTiles.size();
}

void Awareness::pruneTiles()
{
    //remove any tiles that aren't used
    if (mActiveTileList->size() > mAwareTiles.size()) {
        if (mActiveTileList->size() > mDesiredTilesAmount) {
            //debug_print("Pruning tiles. Number of active tiles: " << mActiveTileList->size() << ". Number of aware tiles: " << mAwareTiles.size() << " Desired amount: " << mDesiredTilesAmount);
            std::pair<int, int> entry = mActiveTileList->pop_back();

            dtCompressedTileRef tilesRefs[MAX_LAYERS];
            const int ntiles = mTileCache->getTilesAt(entry.first, entry.second, tilesRefs, MAX_LAYERS);
            for (int i = 0; i < ntiles; ++i) {
                const dtCompressedTile* tile = mTileCache->getTileByRef(tilesRefs[i]);
                float min[3];
                int tx = tile->header->tx;
                int ty = tile->header->ty;
                int tlayer = tile->header->tlayer;
                rcVcopy(min, tile->header->bmin);
                mTileCache->removeTile(tilesRefs[i], NULL, NULL);
                mNavMesh->removeTile(mNavMesh->getTileRefAt(tx, ty, tlayer), 0, 0);

                EventTileRemoved(tx, ty, tlayer);
            }

        }
    }
}

bool Awareness::needsPruning() const
{
    return (mActiveTileList->size() > mDesiredTilesAmount) && (mActiveTileList->size() > mAwareTiles.size());
}

void Awareness::setDesiredTilesAmount(size_t amount)
{
    mDesiredTilesAmount = amount;
}

float Awareness::getTileSizeInMeters() const
{
    return mCfg.tileSize * mCfg.cs;
}

bool Awareness::isPositionAware(float x, float y) const
{
    float tilesize = mCfg.tileSize * mCfg.cs;
    std::pair<int, int> tileIndex((x - mCfg.bmin[0]) / tilesize, (y - mCfg.bmin[2]) / tilesize);
    return mAwareTiles.find(tileIndex) != mAwareTiles.end();
}


void Awareness::findAffectedTiles(const WFMath::AxisBox<2>& area, int& tileMinXIndex, int& tileMaxXIndex, int& tileMinYIndex, int& tileMaxYIndex) const
{
    float tilesize = mCfg.tileSize * mCfg.cs;
    WFMath::Point<2> lowCorner = area.lowCorner();
    WFMath::Point<2> highCorner = area.highCorner();

    if (lowCorner.x() < mCfg.bmin[0]) {
        lowCorner.x() = mCfg.bmin[0];
    }
    if (lowCorner.y() < mCfg.bmin[2]) {
        lowCorner.y() = mCfg.bmin[2];
    }
    if (lowCorner.x() > mCfg.bmax[0]) {
        lowCorner.x() = mCfg.bmax[0];
    }
    if (lowCorner.y() > mCfg.bmax[2]) {
        lowCorner.y() = mCfg.bmax[2];
    }

    if (highCorner.x() < mCfg.bmin[0]) {
        highCorner.x() = mCfg.bmin[0];
    }
    if (highCorner.y() < mCfg.bmin[2]) {
        highCorner.y() = mCfg.bmin[2];
    }
    if (highCorner.x() > mCfg.bmax[0]) {
        highCorner.x() = mCfg.bmax[0];
    }
    if (highCorner.y() > mCfg.bmax[2]) {
        highCorner.y() = mCfg.bmax[2];
    }

    tileMinXIndex = (lowCorner.x() - mCfg.bmin[0]) / tilesize;
    tileMaxXIndex = (highCorner.x() - mCfg.bmin[0]) / tilesize;
    tileMinYIndex = (lowCorner.y() - mCfg.bmin[2]) / tilesize;
    tileMaxYIndex = (highCorner.y() - mCfg.bmin[2]) / tilesize;
}

int Awareness::findPath(const WFMath::Point<3>& start, const WFMath::Point<3>& end, float radius, std::list<WFMath::Point<3>>& path) const
{

    float pStartPos[] { start.x(), start.z(), start.y() };
    float pEndPos[] { end.x(), end.z(), end.y() };
    float startExtent[] { mAgentRadius * 2.2f, 100, mAgentRadius  * 2.2f}; //Only extend radius in horizontal plane
    //To make sure that the agent can move close enough we need to subtract the agent's radius from the destination radius.
    //We'll also adjust with 0.95 to allow for some padding.
    float destinationRadius = (radius - mAgentRadius) * 0.95f;
    float endExtent[] { destinationRadius, 100, destinationRadius}; //Only extend radius in horizontal plane


    dtStatus status;
    dtPolyRef StartPoly;
    float StartNearest[3];
    dtPolyRef EndPoly;
    float EndNearest[3];
    dtPolyRef PolyPath[MAX_PATHPOLY];
    int nPathCount = 0;
    float StraightPath[MAX_PATHVERT * 3];
    int nVertCount = 0;

// find the start polygon
    status = mNavQuery->findNearestPoly(pStartPos, startExtent, mFilter, &StartPoly, StartNearest);
    if ((status & DT_FAILURE) || StartPoly == 0)
        return -1; // couldn't find a polygon

// find the end polygon
    status = mNavQuery->findNearestPoly(pEndPos, endExtent, mFilter, &EndPoly, EndNearest);
    if ((status & DT_FAILURE) || EndPoly == 0)
        return -2; // couldn't find a polygon

    status = mNavQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, mFilter, PolyPath, &nPathCount, MAX_PATHPOLY);
    if ((status & DT_FAILURE))
        return -3; // couldn't create a path
    if (nPathCount == 0)
        return -4; // couldn't find a path

    status = mNavQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, NULL, NULL, &nVertCount, MAX_PATHVERT);
    if ((status & DT_FAILURE))
        return -5; // couldn't create a path
    if (nVertCount == 0)
        return -6; // couldn't find a path

// At this point we have our path.
    for (int nVert = 0; nVert < nVertCount; nVert++) {
        path.emplace_back(StraightPath[nVert * 3], StraightPath[(nVert * 3) + 2], StraightPath[(nVert * 3) + 1]);
    }

    return nVertCount;
}

bool Awareness::projectPosition(int entityId, WFMath::Point<3>& pos, double currentServerTimestamp)
{
    auto entityI = mObservedEntities.find(entityId);
    if (entityI != mObservedEntities.end()) {
        auto& entityEntry = entityI->second;
        pos = entityEntry->location.m_pos;
        const auto& velocity = entityEntry->location.m_velocity;
        if (velocity.isValid() && velocity != WFMath::Vector<3>::ZERO()) {
            pos += (velocity * (currentServerTimestamp - entityEntry->location.timeStamp()));
        }
        return true;
    }
    return false;
}


void Awareness::setAwarenessArea(const std::string& areaId, const WFMath::RotBox<2>& area, const WFMath::Segment<2>& focusLine)
{

    auto& awareAreaSet = mAwareAreas[areaId];

    std::set<std::pair<int, int>> newAwareAreaSet;

    WFMath::AxisBox<2> axisbox = area.boundingBox();

//adjust area to fit with tiles

    float tilesize = mCfg.tileSize * mCfg.cs;
    WFMath::Point<2> lowCorner = axisbox.lowCorner();
    WFMath::Point<2> highCorner = axisbox.highCorner();

    if (lowCorner.x() < mCfg.bmin[0]) {
        lowCorner.x() = mCfg.bmin[0];
    }
    if (lowCorner.y() < mCfg.bmin[2]) {
        lowCorner.y() = mCfg.bmin[2];
    }
    if (lowCorner.x() > mCfg.bmax[0]) {
        lowCorner.x() = mCfg.bmax[0];
    }
    if (lowCorner.y() > mCfg.bmax[2]) {
        lowCorner.y() = mCfg.bmax[2];
    }

    if (highCorner.x() < mCfg.bmin[0]) {
        highCorner.x() = mCfg.bmin[0];
    }
    if (highCorner.y() < mCfg.bmin[2]) {
        highCorner.y() = mCfg.bmin[2];
    }
    if (highCorner.x() > mCfg.bmax[0]) {
        highCorner.x() = mCfg.bmax[0];
    }
    if (highCorner.y() > mCfg.bmax[2]) {
        highCorner.y() = mCfg.bmax[2];
    }

    int tileMinXIndex = (lowCorner.x() - mCfg.bmin[0]) / tilesize;
    int tileMaxXIndex = (highCorner.x() - mCfg.bmin[0]) / tilesize;
    int tileMinYIndex = (lowCorner.y() - mCfg.bmin[2]) / tilesize;
    int tileMaxYIndex = (highCorner.y() - mCfg.bmin[2]) / tilesize;

//Now mark tiles
    const float tcs = mCfg.tileSize * mCfg.cs;
    const float tileBorderSize = mCfg.borderSize * mCfg.cs;

    bool wereDirtyTiles = !mDirtyAwareTiles.empty();
    for (int tx = tileMinXIndex; tx <= tileMaxXIndex; ++tx) {
        for (int ty = tileMinYIndex; ty <= tileMaxYIndex; ++ty) {
            // Tile bounds.
            WFMath::AxisBox<2> tileBounds(WFMath::Point<2>((mCfg.bmin[0] + tx * tcs) - tileBorderSize, (mCfg.bmin[2] + ty * tcs) - tileBorderSize),
                    WFMath::Point<2>((mCfg.bmin[0] + (tx + 1) * tcs) + tileBorderSize, (mCfg.bmin[2] + (ty + 1) * tcs) + tileBorderSize));
            if (WFMath::Intersect(area, tileBounds, false) || WFMath::Contains(area, tileBounds, false)) {

                std::pair<int, int> index(tx, ty);

                newAwareAreaSet.insert(index);
                //If true we should insert in the front of the dirty tiles list.
                bool insertFront = false;
                //If true we should insert in the back of the dirty tiles list.
                bool insertBack = false;
                //If the tile was marked as dirty in the old aware tiles, retain it as such
                if (mDirtyAwareTiles.find(index) != mDirtyAwareTiles.end()) {
                    if (focusLine.isValid() && WFMath::Intersect(focusLine, tileBounds, false)) {
                        insertFront = true;
                    } else {
                        insertBack = true;
                    }
                } else if (mDirtyUnwareTiles.find(index) != mDirtyUnwareTiles.end()) {
                    //if the tile was marked as dirty in the unaware tiles we'll move it to the dirty aware collection.
                    if (focusLine.isValid() && WFMath::Intersect(focusLine, tileBounds, false)) {
                        insertFront = true;
                    } else {
                        insertBack = true;
                    }
                } else {
                    //The tile wasn't marked as dirty in any set, but it might be that it hasn't been processed before.
                    auto tile = mTileCache->getTileAt(tx, ty, 0);
                    if (!tile) {
                        if (focusLine.isValid() && WFMath::Intersect(focusLine, tileBounds, false)) {
                            insertFront = true;
                        } else {
                            insertBack = true;
                        }
                    }
                }

                if (insertFront) {
                    if (mDirtyAwareTiles.insert(index).second) {
                        mDirtyAwareOrderedTiles.push_front(index);
                    }
                } else if (insertBack) {
                    if (mDirtyAwareTiles.insert(index).second) {
                        mDirtyAwareOrderedTiles.push_back(index);
                    }
                }

                mDirtyUnwareTiles.erase(index);

                auto existingAwareTileI = awareAreaSet.find(index);
                if (existingAwareTileI == awareAreaSet.end()) {
                    //Tile wasn't part of the existing set; increase count
                    mAwareTiles[index]++;
                } else {
                    //Tile was part of the existing set. No need to increase aware count,
                    //but remove from awareAreaSet to avoid count being decreased once we're done
                    awareAreaSet.erase(existingAwareTileI);
                }
                newAwareAreaSet.insert(index);

                mActiveTileList->insert(index);
            }
        }
    }

    //All tiles that still are in awareAreaSet are those that aren't active anymore.
    //Aware count should be decreased for each one.
    returnAwareTiles(awareAreaSet);

    //Finally copy the new aware area set into the set
    awareAreaSet = newAwareAreaSet;


    debug_print("Awareness area set: " << area << ". Dirty unaware tiles: " << mDirtyUnwareTiles.size() << " Dirty aware tiles: " << mDirtyAwareTiles.size() << " Aware tile count: " << mAwareTiles.size());

    if (!wereDirtyTiles && !mDirtyAwareTiles.empty()) {
        EventTileDirty();
    }
}

void Awareness::returnAwareTiles(const std::set<std::pair<int,int>>& tileset)
{
    for (auto& tileIndex : tileset) {
        auto awareEntry = mAwareTiles.find(tileIndex);
        awareEntry->second--;
        if (awareEntry->second == 0) {
            mAwareTiles.erase(awareEntry);
            if (mDirtyAwareTiles.erase(tileIndex)) {
                mDirtyAwareOrderedTiles.remove(tileIndex);
                mDirtyUnwareTiles.insert(tileIndex);
            }
        }
    }
}


void Awareness::removeAwarenessArea(const std::string& areaId)
{
    auto I = mAwareAreas.find(areaId);
    if (I == mAwareAreas.end()) {
        return;
    }

    returnAwareTiles(I->second);
}


size_t Awareness::unawareTilesInArea(const std::string& areaId) const
{
    auto I = mAwareAreas.find(areaId);
    if (I == mAwareAreas.end()) {
        return 0;
    }

    size_t count = 0;
    auto& tileSet = I->second;
    for (auto& entry : tileSet) {
        if (mDirtyAwareTiles.find(entry) == tileSet.end()) {
            ++count;
        }
    }
    return count;
}


void Awareness::rebuildTile(int tx, int ty, const std::vector<WFMath::RotBox<2>>& entityAreas)
{
    TileCacheData tiles[MAX_LAYERS];
    memset(tiles, 0, sizeof(tiles));

    int ntiles = rasterizeTileLayers(entityAreas, tx, ty, tiles, MAX_LAYERS);

    for (int j = 0; j < ntiles; ++j) {
        TileCacheData* tile = &tiles[j];

        dtTileCacheLayerHeader* header = (dtTileCacheLayerHeader*)tile->data;
        dtTileRef tileRef = mTileCache->getTileRef(mTileCache->getTileAt(header->tx, header->ty, header->tlayer));
        if (tileRef) {
            mTileCache->removeTile(tileRef, NULL, NULL);
        }
        dtStatus status = mTileCache->addTile(tile->data, tile->dataSize, DT_COMPRESSEDTILE_FREE_DATA, 0); // Add compressed tiles to tileCache
        if (dtStatusFailed(status)) {
            log(WARNING, String::compose("Failed to add tile in awareness. x: %1 y: %2 Reason: %3", tx, ty, status));
            dtFree(tile->data);
            tile->data = 0;
            continue;
        }
    }

    dtStatus status = mTileCache->buildNavMeshTilesAt(tx, ty, mNavMesh);
    if (dtStatusFailed(status)) {
        log(WARNING, String::compose("Failed to build nav mesh tile in awareness. x: %1 y: %2 Reason: %3", tx, ty, status));
    }

    EventTileUpdated(tx, ty);

}

void Awareness::buildEntityAreas(const EntityEntry& entity, std::map<const EntityEntry*, WFMath::RotBox<2>>& entityAreas)
{

    //The entity is solid (i.e. can be collided with) if it has a bbox and the "solid" property isn't set to false (or 0 as it's an int).
    bool isSolid = entity.location.bBox().isValid() && entity.location.isSolid();

    if (isSolid) {
        //we now have to get the location of the entity in world space
        const WFMath::Point<3>& pos = entity.location.pos();
        const WFMath::Quaternion& orientation = entity.location.orientation();

        if (pos.isValid() && orientation.isValid()) {

            WFMath::Vector<3> xVec = WFMath::Vector<3>(1.0, 0.0, 0.0).rotate(orientation);
            double theta = atan2(xVec.y(), xVec.x()); // rotation about Z

            WFMath::RotMatrix<2> rm;
            rm.rotation(theta);

            const BBox& bbox = entity.location.m_bBox;

            WFMath::Point<2> highCorner(bbox.highCorner().x(), bbox.highCorner().y());
            WFMath::Point<2> lowCorner(bbox.lowCorner().x(), bbox.lowCorner().y());

            //Expand the box a little so that we can navigate around it without being stuck on it.
            //We'll the radius of the avatar.
            highCorner += WFMath::Vector<2>(mAgentRadius, mAgentRadius);
            lowCorner -= WFMath::Vector<2>(mAgentRadius, mAgentRadius);

            WFMath::RotBox<2> rotbox(WFMath::Point<2>::ZERO(), highCorner - lowCorner, WFMath::RotMatrix<2>().identity());
            rotbox.shift(WFMath::Vector<2>(lowCorner.x(), lowCorner.y()));
            rotbox.rotatePoint(rm, WFMath::Point<2>::ZERO());

            rotbox.shift(WFMath::Vector<2>(pos.x(), pos.y()));

            entityAreas.insert(std::make_pair(&entity, rotbox));
        }
    }
}

void Awareness::findEntityAreas(const WFMath::AxisBox<2>& extent, std::vector<WFMath::RotBox<2> >& areas)
{
    for (auto& entry : mEntityAreas) {
        auto& rotbox = entry.second;
        if (WFMath::Contains(extent, rotbox, false) || WFMath::Intersect(extent, rotbox, false)) {
            areas.push_back(rotbox);
        }
    }
}

int Awareness::rasterizeTileLayers(const std::vector<WFMath::RotBox<2>>& entityAreas, const int tx, const int ty, TileCacheData* tiles, const int maxTiles)
{
    std::vector<float> vertsVector;
    std::vector<int> trisVector;

    FastLZCompressor comp;
    RasterizationContext rc;

// Tile bounds.
    const float tcs = mCfg.tileSize * mCfg.cs;

    rcConfig tcfg;
    memcpy(&tcfg, &mCfg, sizeof(tcfg));

    tcfg.bmin[0] = mCfg.bmin[0] + tx * tcs;
    tcfg.bmin[1] = mCfg.bmin[1];
    tcfg.bmin[2] = mCfg.bmin[2] + ty * tcs;
    tcfg.bmax[0] = mCfg.bmin[0] + (tx + 1) * tcs;
    tcfg.bmax[1] = mCfg.bmax[1];
    tcfg.bmax[2] = mCfg.bmin[2] + (ty + 1) * tcs;
    tcfg.bmin[0] -= tcfg.borderSize * tcfg.cs;
    tcfg.bmin[2] -= tcfg.borderSize * tcfg.cs;
    tcfg.bmax[0] += tcfg.borderSize * tcfg.cs;
    tcfg.bmax[2] += tcfg.borderSize * tcfg.cs;

//First define all vertices. Get one extra vertex in each direction so that there's no cutoff at the tile's edges.
    int heightsXMin = std::floor(tcfg.bmin[0]) - 1;
    int heightsXMax = std::ceil(tcfg.bmax[0]) + 1;
    int heightsYMin = std::floor(tcfg.bmin[2]) - 1;
    int heightsYMax = std::ceil(tcfg.bmax[2]) + 1;
    int sizeX = heightsXMax - heightsXMin;
    int sizeY = heightsYMax - heightsYMin;

//Blit height values with 1 meter interval
    std::vector<float> heights(sizeX * sizeY);
    mHeightProvider.blitHeights(heightsXMin, heightsXMax, heightsYMin, heightsYMax, heights);

    float* heightData = heights.data();
    for (int y = heightsYMin; y < heightsYMax; ++y) {
        for (int x = heightsXMin; x < heightsXMax; ++x) {
            vertsVector.push_back(x);
            vertsVector.push_back(*heightData);
            vertsVector.push_back(y);
            heightData++;
        }
    }

//Then define the triangles
    for (int y = 0; y < (sizeY - 1); y++) {
        for (int x = 0; x < (sizeX - 1); x++) {
            size_t vertPtr = (y * sizeX) + x;
            //make a square, including the vertices to the right and below
            trisVector.push_back(vertPtr);
            trisVector.push_back(vertPtr + sizeX);
            trisVector.push_back(vertPtr + 1);

            trisVector.push_back(vertPtr + 1);
            trisVector.push_back(vertPtr + sizeX);
            trisVector.push_back(vertPtr + 1 + sizeX);
        }
    }

    float* verts = vertsVector.data();
    int* tris = trisVector.data();
    const int nverts = vertsVector.size() / 3;
    const int ntris = trisVector.size() / 3;

// Allocate voxel heightfield where we rasterize our input data to.
    rc.solid = rcAllocHeightfield();
    if (!rc.solid) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'solid'.");
        return 0;
    }
    if (!rcCreateHeightfield(mCtx, *rc.solid, tcfg.width, tcfg.height, tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch)) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
        return 0;
    }

// Allocate array that can hold triangle flags.
    rc.triareas = new unsigned char[ntris];
    if (!rc.triareas) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'm_triareas' (%d).", ntris / 3);
        return 0;
    }

    memset(rc.triareas, 0, ntris * sizeof(unsigned char));
    rcMarkWalkableTriangles(mCtx, tcfg.walkableSlopeAngle, verts, nverts, tris, ntris, rc.triareas);

    rcRasterizeTriangles(mCtx, verts, nverts, tris, rc.triareas, ntris, *rc.solid, tcfg.walkableClimb);

// Once all geometry is rasterized, we do initial pass of filtering to
// remove unwanted overhangs caused by the conservative rasterization
// as well as filter spans where the character cannot possibly stand.

//NOTE: These are disabled for now since we currently only handle a simple 2d height map
//with bounding boxes snapped to the ground. If this changes these calls probably needs to be activated.
//	rcFilterLowHangingWalkableObstacles(m_ctx, tcfg.walkableClimb, *rc.solid);
//	rcFilterLedgeSpans(m_ctx, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid);
//	rcFilterWalkableLowHeightSpans(m_ctx, tcfg.walkableHeight, *rc.solid);

    rc.chf = rcAllocCompactHeightfield();
    if (!rc.chf) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'chf'.");
        return 0;
    }
    if (!rcBuildCompactHeightfield(mCtx, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid, *rc.chf)) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
        return 0;
    }

// Erode the walkable area by agent radius.
    if (!rcErodeWalkableArea(mCtx, tcfg.walkableRadius, *rc.chf)) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not erode.");
        return 0;
    }

// Mark areas.
    for (auto& rotbox : entityAreas) {
        float verts[3 * 4];

        verts[0] = rotbox.getCorner(1).x();
        verts[1] = 0;
        verts[2] = rotbox.getCorner(1).y();

        verts[3] = rotbox.getCorner(3).x();
        verts[4] = 0;
        verts[5] = rotbox.getCorner(3).y();

        verts[6] = rotbox.getCorner(2).x();
        verts[7] = 0;
        verts[8] = rotbox.getCorner(2).y();

        verts[9] = rotbox.getCorner(0).x();
        verts[10] = 0;
        verts[11] = rotbox.getCorner(0).y();

        rcMarkConvexPolyArea(mCtx, verts, 4, tcfg.bmin[1], tcfg.bmax[1], DT_TILECACHE_NULL_AREA, *rc.chf);
    }

    rc.lset = rcAllocHeightfieldLayerSet();
    if (!rc.lset) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'lset'.");
        return 0;
    }
    if (!rcBuildHeightfieldLayers(mCtx, *rc.chf, tcfg.borderSize, tcfg.walkableHeight, *rc.lset)) {
        mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not build heighfield layers.");
        return 0;
    }

    rc.ntiles = 0;
    for (int i = 0; i < rcMin(rc.lset->nlayers, MAX_LAYERS); ++i) {
        TileCacheData* tile = &rc.tiles[rc.ntiles++];
        const rcHeightfieldLayer* layer = &rc.lset->layers[i];

        // Store header
        dtTileCacheLayerHeader header;
        header.magic = DT_TILECACHE_MAGIC;
        header.version = DT_TILECACHE_VERSION;

        // Tile layer location in the navmesh.
        header.tx = tx;
        header.ty = ty;
        header.tlayer = i;
        dtVcopy(header.bmin, layer->bmin);
        dtVcopy(header.bmax, layer->bmax);

        // Tile info.
        header.width = (unsigned char)layer->width;
        header.height = (unsigned char)layer->height;
        header.minx = (unsigned char)layer->minx;
        header.maxx = (unsigned char)layer->maxx;
        header.miny = (unsigned char)layer->miny;
        header.maxy = (unsigned char)layer->maxy;
        header.hmin = (unsigned short)layer->hmin;
        header.hmax = (unsigned short)layer->hmax;

        dtStatus status = dtBuildTileCacheLayer(&comp, &header, layer->heights, layer->areas, layer->cons, &tile->data, &tile->dataSize);
        if (dtStatusFailed(status)) {
            return 0;
        }
    }

// Transfer ownership of tile data from build context to the caller.
    int n = 0;
    for (int i = 0; i < rcMin(rc.ntiles, maxTiles); ++i) {
        tiles[n++] = rc.tiles[i];
        rc.tiles[i].data = 0;
        rc.tiles[i].dataSize = 0;
    }

    return n;
}

void Awareness::processTiles(const WFMath::AxisBox<2>& area,
        const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const
{
    float bmin[] { area.lowCorner().x(), -100, area.lowCorner().y() };
    float bmax[] { area.highCorner().x(), 100, area.highCorner().y() };

    dtCompressedTileRef tilesRefs[256];
    int ntiles;
    dtStatus status = mTileCache->queryTiles(bmin, bmax, tilesRefs, &ntiles, 256);
    if (status == DT_SUCCESS) {
        std::vector<const dtCompressedTile*> tiles(ntiles);
        for (int i = 0; i < ntiles; ++i) {
            tiles[i] = mTileCache->getTileByRef(tilesRefs[i]);
        }
        processTiles(tiles, processor);
    }
}

void Awareness::processTile(const int tx, const int ty,
        const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const
{
    dtCompressedTileRef tilesRefs[MAX_LAYERS];
    const int ntiles = mTileCache->getTilesAt(tx, ty, tilesRefs, MAX_LAYERS);

    std::vector<const dtCompressedTile*> tiles(ntiles);
    for (int i = 0; i < ntiles; ++i) {
        tiles[i] = mTileCache->getTileByRef(tilesRefs[i]);
    }

    processTiles(tiles, processor);
}

void Awareness::processAllTiles(
        const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const
{
    int ntiles = mTileCache->getTileCount();
    std::vector<const dtCompressedTile*> tiles(ntiles);
    for (int i = 0; i < ntiles; ++i) {
        tiles[i] = mTileCache->getTile(i);
    }

    processTiles(tiles, processor);

}

void Awareness::processTiles(std::vector<const dtCompressedTile*> tiles,
        const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const
{
    struct TileCacheBuildContext
    {
            inline TileCacheBuildContext(struct dtTileCacheAlloc* a) :
                    layer(0), lcset(0), lmesh(0), alloc(a)
            {
            }
            inline ~TileCacheBuildContext()
            {
                purge();
            }
            void purge()
            {
                dtFreeTileCacheLayer(alloc, layer);
                layer = 0;
                dtFreeTileCacheContourSet(alloc, lcset);
                lcset = 0;
                dtFreeTileCachePolyMesh(alloc, lmesh);
                lmesh = 0;
            }
            struct dtTileCacheLayer* layer;
            struct dtTileCacheContourSet* lcset;
            struct dtTileCachePolyMesh* lmesh;
            struct dtTileCacheAlloc* alloc;
    };

    dtTileCacheAlloc* talloc = mTileCache->getAlloc();
    dtTileCacheCompressor* tcomp = mTileCache->getCompressor();
    const dtTileCacheParams* params = mTileCache->getParams();

    for (const dtCompressedTile* tile : tiles) {

        talloc->reset();

        TileCacheBuildContext bc(talloc);
        const int walkableClimbVx = (int)(params->walkableClimb / params->ch);
        dtStatus status;

        // Decompress tile layer data.
        status = dtDecompressTileCacheLayer(talloc, tcomp, tile->data, tile->dataSize, &bc.layer);
        if (dtStatusFailed(status))
            return;

        // Build navmesh
        status = dtBuildTileCacheRegions(talloc, *bc.layer, walkableClimbVx);
        if (dtStatusFailed(status))
            return;

        bc.lcset = dtAllocTileCacheContourSet(talloc);
        if (!bc.lcset)
            return;
        status = dtBuildTileCacheContours(talloc, *bc.layer, walkableClimbVx, params->maxSimplificationError, *bc.lcset);
        if (dtStatusFailed(status))
            return;

        bc.lmesh = dtAllocTileCachePolyMesh(talloc);
        if (!bc.lmesh)
            return;
        status = dtBuildTileCachePolyMesh(talloc, *bc.lcset, *bc.lmesh);
        if (dtStatusFailed(status))
            return;

        processor(mTileCache->getTileRef(tile), *bc.lmesh, tile->header->bmin, params->cs, params->ch, *bc.layer);

    }
}

