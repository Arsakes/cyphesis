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

#include "common/compose.hpp"
#include "common/log.h"
#include "common/globals.h"
#include "common/sockets.h"
#include "common/system.h"
#include "common/AtlasStreamClient.h"

#include "EntityImporter.h"
#include "AgentCreationTask.h"
#include "EntityTraversalTask.h"
#include "WaitForDeletionTask.h"

#include <varconf/config.h>

using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Root;
using Atlas::Objects::smart_dynamic_cast;
using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Operation::Talk;
using Atlas::Objects::Operation::Get;
using Atlas::Message::Element;
using Atlas::Message::ListType;
using Atlas::Message::MapType;

//Note that we check for the existence of these keys instead
//of inspecting the registered variables, since we want to make it easy
//for users, just having them supply the flag rather than setting the
//value to 1. I.e. rather "--clear" than "--clear=1"
BOOL_OPTION(_clear, false, "", "clear",
        "Delete all existing entities before importing.")
BOOL_OPTION(_merge, false, "", "merge",
        "Try to merge contents in export with existing entities.")
BOOL_OPTION(_resume, false, "", "resume",
        "If the world is suspended, resume after import.")

static void usage(char * prg)
{
    std::cerr << "usage: " << prg << " [options] filepath" << std::endl
            << std::flush;
}

int main(int argc, char ** argv)
{
    int config_status = loadConfig(argc, argv, USAGE_CYCMD);
    if (config_status < 0) {
        if (config_status == CONFIG_VERSION) {
            reportVersion(argv[0]);
            return 0;
        } else if (config_status == CONFIG_HELP) {
            showUsage(argv[0], USAGE_CYCMD);
            return 0;
        } else if (config_status != CONFIG_ERROR) {
            log(ERROR, "Unknown error reading configuration.");
        }
        // Fatal error loading config file
        return 1;
    }

    std::string filename;
    int optind = config_status;
    if ((argc - optind) == 1) {
        filename = argv[optind];
    } else {
        usage(argv[0]);
        return 1;
    }

    std::string server;
    readConfigItem("client", "serverhost", server);

    int useslave = 0;
    readConfigItem("client", "useslave", useslave);

    AtlasStreamClient bridge;
    std::string localSocket;
    if (useslave != 0) {
        localSocket = slave_socket_name;
    } else {
        localSocket = client_socket_name;
    }

    std::cout << "Attempting local connection" << std::flush;
    if (bridge.connectLocal(localSocket) == 0) {
        if (bridge.create("sys", create_session_username(),
                String::compose("%1%2", ::rand(), ::rand())) != 0) {
            std::cerr << "Could not create sys account." << std::endl
                    << std::flush;
            return -1;
        }
        std::cout << " done." << std::endl << std::flush;
        auto loginInfo = bridge.getInfoReply();
        const std::string accountId = loginInfo->getId();

        std::cout << "Attempting creation of agent " << std::flush;
        std::string agent_id;
        bridge.runTask(new AgentCreationTask(accountId, "creator", agent_id),
                "");
        if (bridge.pollUntilTaskComplete() != 0) {
            std::cerr << "Could not create agent." << std::endl << std::flush;
            return -1;
        }
        if (agent_id == "") {
            std::cerr << "Could not create agent; no id received." << std::endl
                    << std::flush;
            return -1;
        }
        std::cout << "done." << std::endl << std::flush;

        //Check to see if the world is empty. This is done by looking for any entity that's not
        //the root one and that isn't transient.
        bool clear = varconf::Config::inst()->find("", "clear");
        bool merge = varconf::Config::inst()->find("", "merge");

        bool resume = varconf::Config::inst()->find("", "resume");

        if (clear && merge) {
            std::cerr
                    << "'--clear' and '--merge' are mutually exclusive; you can't specify both."
                    << std::endl << std::flush;
            return -1;
        }

        if (!clear && !merge) {
            bool isPopulated = false;
            std::function<bool(const RootEntity&)> visitor =
                    [&](const RootEntity& entity)->bool {
                        if (entity->getId() != "0" && entity->getId() != agent_id && !entity->hasAttr("transient")) {
                            isPopulated = true;
                            return false;
                        }
                        return true;
                    };

            std::cout << "Checking if world already is populated."  << std::endl << std::flush;
            EntityTraversalTask* populationCheck = new EntityTraversalTask(
                    accountId, visitor);
            bridge.runTask(populationCheck, "0");
            if (bridge.pollUntilTaskComplete() != 0) {
                std::cerr
                        << "Error when checking if the server already is populated."
                        << std::endl << std::flush;
                return -1;
            }

            if (isPopulated) {
                std::cerr << "Server is already populated, aborting.\n"
                        "Either use the "
                        "'--clear' flag to first clear it. This "
                        "will delete all existing entities.\nOr "
                        "use the '--merge' flag to merge the "
                        "entities in the export with the existing"
                        " ones. The results of this are not always"
                        " predictable though." << std::endl << std::flush;
                return -1;
            }
        }

        if (clear) {
            std::cout << "Clearing world first." << std::endl << std::flush;
            //Tell the world to clear itself
            Anonymous deleteArg;
            deleteArg->setId("0");
            deleteArg->setAttr("force", 1);
            Atlas::Objects::Operation::Delete deleteOp;
            deleteOp->setTo("0");
            deleteOp->setFrom(agent_id);
            deleteOp->setArgs1(deleteArg);

            bridge.send(deleteOp);

            std::cout << "Waiting for world to be cleared." << std::endl << std::flush;
            //Wait for the agent to be deleted.
            bridge.runTask(new WaitForDeletionTask(agent_id), "");
            if (bridge.pollUntilTaskComplete() != 0) {
                std::cerr << "Error when waiting for world to be cleared." << std::endl
                        << std::flush;
                return -1;
            }

            std::cout << "World is cleared; creating new agent." << std::endl << std::flush;

            //Once the world has been cleared we need to create a new agent,
            //since the first one got deleted
            bridge.runTask(
                    new AgentCreationTask(accountId, "creator", agent_id), "");
            if (bridge.pollUntilTaskComplete() != 0) {
                std::cerr << "Could not create agent." << std::endl
                        << std::flush;
                return -1;
            }
            if (agent_id == "") {
                std::cerr << "Could not create agent; no id received."
                        << std::endl << std::flush;
                return -1;
            }
        }

        std::cout << "Starting import." << std::endl << std::flush;

        //Ownership of this is transferred to the bridge when it's run, so we shouldn't delete it
        auto importer = new EntityImporter(accountId, agent_id);

        importer->setResume(resume);

        bridge.runTask(importer, filename);
        if (bridge.pollUntilTaskComplete() != 0) {
            std::cerr << "Could not import." << std::endl << std::flush;
            return -1;
        }

        std::cout << "Import done." << std::endl << std::flush;
        return 0;
    }

    return 0;

}

