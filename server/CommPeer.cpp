// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2004 Alistair Riddoch
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


#include "CommPeer.h"
#include "CommAsioClient_impl.h"

#include "Peer.h"

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Anonymous.h>

using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Operation::Info;

/// \brief Constructor remote peer socket object.
///
/// @param svr Reference to the object that manages all socket communication.
/// @param name The name of the peer instance.
/// @param password Password to login with on peer
CommPeer::CommPeer(const std::string & name,
        boost::asio::io_service& io_service) :
        CommAsioClient<boost::asio::ip::tcp>(name, io_service), m_auth_timer(io_service)
{
}

CommPeer::~CommPeer()
{
}

/// \brief Connect to a remote peer on a specific port
///
/// @param host The hostname of the peer to connect to
/// @param port The port to connect on
void CommPeer::connect(const std::string & host, int port)
{
    connect(
            boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::address::from_string(host), port));
}

/// \brief Connect to a remote peer wath a specific address info
///
/// @param endpoint The address info of the peer to connect to
void CommPeer::connect(const boost::asio::ip::tcp::endpoint& endpoint)
{
    auto self(this->shared_from_this());
    mSocket.async_connect(endpoint,
            [this, self](const boost::system::error_code& ec) {
                if (!ec) {
                    connected.emit();
                } else {
                    failed.emit();
                }

            });
}

void CommPeer::setup(Link * connection)
{
    startConnect(connection);

    m_start_auth = boost::posix_time::microsec_clock::local_time();
    m_auth_timer.expires_from_now(boost::posix_time::seconds(1));
    m_auth_timer.async_wait([this](const boost::system::error_code& ec){
        if (!ec) {
            this->checkAuth();
        }
    });
}

/// \brief Called periodically by the server
///
/// \param t The current time at the time of calling
void CommPeer::checkAuth()
{
    // Wait for the negotiation to finish with the peer
    if (m_negotiate != 0) {
        if ((boost::posix_time::microsec_clock::local_time() - m_start_auth).seconds()
                > 10) {
            log(NOTICE, "Client disconnected because of negotiation timeout.");
            mSocket.cancel();
            return;
        }
    } else {
        Peer *peer = dynamic_cast<Peer*>(m_link);
        if (peer == NULL) {
            log(WARNING, "Casting CommPeer connection to Peer failed");
            return;
        }
        // Check if we have been stuck in a state of authentication in-progress
        // for over 20 seconds. If so, disconnect from and remove peer.
        if ((boost::posix_time::microsec_clock::local_time() - m_start_auth).seconds()
                > 20) {
            if (peer->getAuthState() == PEER_AUTHENTICATING) {
                log(NOTICE,
                        "Peer disconnected because authentication timed out.");
                mSocket.cancel();
                return;
            }
        }
        if (peer->getAuthState() == PEER_AUTHENTICATED) {
            peer->cleanTeleports();
            return;
        }
    }
    m_auth_timer.expires_from_now(boost::posix_time::seconds(1));
    m_auth_timer.async_wait([this](const boost::system::error_code& ec){
        this->checkAuth();
    });
}
