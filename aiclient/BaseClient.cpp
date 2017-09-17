// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000-2005 Alistair Riddoch
// Copyright (C) 2013 Erik Ogenvik
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

#include "BaseClient.h"

#include "common/log.h"
#include "common/debug.h"
#include "common/compose.hpp"
#include "common/system.h"

#include <Atlas/Objects/Anonymous.h>

#include <iostream>

using Atlas::Message::MapType;
using Atlas::Objects::Root;
using Atlas::Objects::Operation::Login;
using Atlas::Objects::Operation::Logout;
using Atlas::Objects::Operation::Create;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Entity::Anonymous;

using Atlas::Objects::smart_dynamic_cast;

using String::compose;

static const bool debug_flag = false;

BaseClient::BaseClient()
{
}

BaseClient::~BaseClient()
{
}

/// \brief Send an operation to the server
void BaseClient::send(const Operation & op)
{
    m_connection.send(op);
}

/// \brief Create a new account on the server
///
/// @param name User name of the new account
/// @param password Password of the new account
Root BaseClient::createSystemAccount(const std::string& usernameSuffix)
{
    Anonymous player_ent;
    m_username = create_session_username() + usernameSuffix;
    player_ent->setAttr("username", m_username);
    m_password = compose("%1%2", ::rand(), ::rand());
    player_ent->setAttr("password", m_password);
    player_ent->setParent("sys");

    Create createAccountOp;
    createAccountOp->setArgs1(player_ent);
    createAccountOp->setSerialno(m_connection.newSerialNo());
    send(createAccountOp);
    if (m_connection.wait() != 0) {
        std::cerr << "ERROR: Failed to log into server: \""
                << m_connection.errorMessage() << "\"" << std::endl
                << std::flush;
        return Root(0);
    }

    const Root & ent = m_connection.getInfoReply();

    if (!ent->hasAttrFlag(Atlas::Objects::ID_FLAG)) {
        std::cerr << "ERROR: Logged in, but account has no id" << std::endl
                << std::flush;
    } else {
        m_playerId = ent->getId();
    }

    return ent;
}

/// \brief Create a new account on the server
///
/// @param name User name of the new account
/// @param password Password of the new account
Root BaseClient::createAccount(const std::string & name,
        const std::string & password)
{
    m_playerName = name;

    Anonymous player_ent;
    player_ent->setAttr("username", name);
    player_ent->setAttr("password", password);
    player_ent->setParent("player");

    debug(
            std::cout << "Logging " << name << " in with " << password
                    << " as password" << std::endl << std::flush
            ;);

    Login loginAccountOp;
    loginAccountOp->setArgs1(player_ent);
    loginAccountOp->setSerialno(m_connection.newSerialNo());
    send(loginAccountOp);

    if (m_connection.wait() != 0) {
        Create createAccountOp;
        createAccountOp->setArgs1(player_ent);
        createAccountOp->setSerialno(m_connection.newSerialNo());
        send(createAccountOp);
        if (m_connection.wait() != 0) {
            std::cerr << "ERROR: Failed to log into server" << std::endl
                    << std::flush;
            return Root(0);
        }
    }

    const Root & ent = m_connection.getInfoReply();

    if (!ent->hasAttrFlag(Atlas::Objects::ID_FLAG)) {
        std::cerr << "ERROR: Logged in, but account has no id" << std::endl
                << std::flush;
    } else {
        m_playerId = ent->getId();
    }

    return ent;
}

void BaseClient::logout()
{
    Logout logout;
    send(logout);

    if (m_connection.wait() != 0) {
        std::cerr << "ERROR: Failed to logout" << std::endl << std::flush;
    }
}

/// \brief Handle any operations that have arrived from the server
int BaseClient::pollOne(const boost::posix_time::time_duration& duration)
{
    int pollResult = m_connection.pollOne(duration);
    if (pollResult == 0) {
        Operation input;
        while ((input = m_connection.pop()).isValid()) {
            if (input->getClassNo() == Atlas::Objects::Operation::ERROR_NO) {
                log(ERROR,
                        String::compose("Got error from server: %1",
                                getErrorMessage(input)));
            }

            OpVector res;
            operation(input, res);
            OpVector::const_iterator Iend = res.end();
            for (OpVector::const_iterator I = res.begin(); I != Iend; ++I) {
                if (input->hasAttrFlag(
                        Atlas::Objects::Operation::SERIALNO_FLAG)) {
                    (*I)->setRefno(input->getSerialno());
                }
                send(*I);
            }
        }
    }
    return pollResult;
}

std::string BaseClient::getErrorMessage(const Operation & err)
{
    const std::vector<Root>& args = err->getArgs();
    if (args.empty()) {
        return "Unknown error.";
    } else {
        const Root & arg = args.front();
        Atlas::Message::Element message;
        if (arg->copyAttr("message", message) != 0) {
            return "Unknown error.";
        } else {
            if (!message.isString()) {
                return "Unknown error.";
            } else {
                return message.String();
            }
        }
    }
}

int BaseClient::runTask(ClientTask * task, const std::string & arg)
{
    return m_connection.runTask(task, arg);
}

int BaseClient::endTask()
{
    return m_connection.endTask();
}

/**
 * Checks if there's an active task.
 * @return True if there's a task set.
 */
bool BaseClient::hasTask() const
{
    return m_connection.hasTask();
}

/**
 * Poll the server until the current task has completed.
 * @return 0 if successful
 */
int BaseClient::pollUntilTaskComplete()
{
    return m_connection.pollUntilTaskComplete();
}
