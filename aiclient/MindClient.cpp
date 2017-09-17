/*
 Copyright (C) 2013 Erik Ogenvik

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

#include "MindClient.h"

#include "ClientConnection.h"

#include "common/log.h"
#include "common/compose.hpp"
#include "common/debug.h"
#include "common/id.h"
#include "common/ScriptKit.h"
#include "common/Setup.h"
#include "common/TypeNode.h"
#include "common/Tick.h"

#include "rulesets/MindFactory.h"
#include "rulesets/BaseMind.h"

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/SmartPtr.h>

using Atlas::Message::MapType;
using Atlas::Objects::Root;
using Atlas::Objects::Operation::RootOperation;
using Atlas::Objects::Operation::Look;
using Atlas::Objects::Operation::Login;
using Atlas::Objects::Operation::Tick;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Entity::Anonymous;

static const bool debug_flag = false;

MindClient::MindClient(const std::string& id, int intId,
        MindFactory& mindFactory) :
        Router(id, intId), m_mindFactory(mindFactory), m_mind(0), m_nextTick(0)
{

}

MindClient::~MindClient()
{
    if (m_mind) {
        m_mind->decRef();
    }
}

void MindClient::idle(OpVector& res)
{
    //Send a Tick operation every second
    //TODO: make this better and more dynamic
    m_systemTime.update();
    if (m_systemTime.seconds() >= m_nextTick) {
        Atlas::Objects::Operation::Tick tick;

        operationToMind(tick, res);

        m_systemTime.update();
        m_nextTick = m_systemTime.seconds() + 1;
    }

}

void MindClient::takePossession(OpVector& res, ClientConnection& connection,
        std::string& accountId, const std::string& possessEntityId,
        const std::string& possessKey)
{
    log(INFO,
            String::compose("Taking possession of entity with id %1.",
                    possessEntityId));

    m_entityId = possessEntityId;

    Anonymous what;
    what->setId(possessEntityId);
    what->setAttr("possess_key", possessKey);

    Look l;
    l->setFrom(accountId);
    l->setArgs1(what);
    l->setSerialno(connection.newSerialNo());
    res.push_back(l);
}

void MindClient::operationToMind(const Operation & op, OpVector & res)
{
    if (m_mind) {
        OpVector mindRes;
        m_mind->operation(op, mindRes);
        for (Operation& resOp : mindRes) {
            resOp->setFrom(m_mind->getId());
            res.push_back(resOp);
        }
    }
}

void MindClient::operation(const Operation & op, OpVector & res)
{
    if (!m_mind) {
        if (op->getClassNo() == Atlas::Objects::Operation::SIGHT_NO) {
            createMind(op, res);
        } else {
            log(ERROR,
                    String::compose("Unrecognized response to possession: %1",
                            op->getParent()));
        }

    } else {
        operationToMind(op, res);
    }
}

void MindClient::createMind(const Operation & op, OpVector & res)
{

    const std::vector<Root>& args = op->getArgs();
    if (args.empty()) {
        log(ERROR, "no args character create/take response");
        return;
    }

    RootEntity ent = Atlas::Objects::smart_dynamic_cast<RootEntity>(
            args.front());
    if (!ent.isValid()) {
        log(ERROR, "malformed character create/take response");
        return;
    }

    std::string entityId = ent->getId();
    std::string entityType = ent->getParent();

    log(INFO,
            String::compose(
                    "Got info on account, creating mind for entity with id %1 of type %2.",
                    entityId, entityType));
    m_mind = m_mindFactory.newMind(entityId, integerId(entityId));
    //We'll keep a reference to the mind so it isn't deleted willy-nilly
    m_mind->incRef();
    //TODO: setup and get type from Inheritance
    m_mind->setType(new TypeNode(entityType));

    if (m_mindFactory.m_scriptFactory != 0) {
        log(INFO, "Adding script to entity.");
        m_mindFactory.m_scriptFactory->addScript(m_mind);
    }

    //Send the Sight operation we just got on to the mind, since it contains info about the entity.
    operationToMind(op, res);

    //Also send a "Setup" op to the mind, which will trigger any setup hooks.
    Atlas::Objects::Operation::Setup s;
    Anonymous setup_arg;
    setup_arg->setName("mind");
    s->setTo(ent->getId());
    s->setArgs1(setup_arg);
    operationToMind(s, res);

    //Start by sending a unspecified "Look". This tells the server to send us a bootstrapped view.
    Look l;
    l->setFrom(entityId);
    res.push_back(l);

}

bool MindClient::isMindDestroyed() const
{
    if (m_mind == nullptr || m_mind->isDestroyed()) {
        return true;
    }
    return false;
}

