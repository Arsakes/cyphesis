// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2003-2004,2006 Alistair Riddoch
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


#include "CommPSQLSocket.h"

#include "common/Database.h"
#include "common/log.h"
#include "common/debug.h"

#include <iostream>

static const bool debug_flag = false;

/// Interval between database vacuum jobs.
const int CommPSQLSocket::vacFreq = 25 * 60;
/// Interval between database reindex jobs.
const int CommPSQLSocket::reindexFreq = 30 * 60;


/// \brief Constructor for PostgreSQL socket polling object.
///
/// @param db Reference to the low level database management object.
CommPSQLSocket::CommPSQLSocket(boost::asio::io_service& io_service, Database & db) :
                               m_io_service(io_service), m_socket(new boost::asio::ip::tcp::socket(io_service)),
                               m_vacuumTimer(io_service), m_reindexTimer(io_service), m_reconnectTimer(io_service),
                               m_db(db), m_vacuumFull(false)
{
    // This assumes the database connection is already sorted, which I think
    // is okay
    PGconn * con = m_db.getConnection();
    assert(con != 0);
    
    if (PQsetnonblocking(con, 1) == -1) {
        log(ERROR, "Unable to put database connection in non-blocking mode.");
    }

    //wrap the postgres socket in our tcp socket
    int fd = PQsocket(con);
    if (fd >= 0) {
        m_socket->assign(boost::asio::ip::tcp::v4(), fd);
    }
    vacuum();
    reindex();
    do_read();
}

void CommPSQLSocket::tryReConnect()
{
    m_reconnectTimer.expires_from_now(boost::posix_time::seconds(2));
    m_reconnectTimer.async_wait([this](boost::system::error_code ec) {
        if (!ec) {
            if (m_db.initConnection() == 0) {
                log(NOTICE, "Database connection re-established");
                PGconn * con = m_db.getConnection();
                if (PQsetnonblocking(con, 1) == -1) {
                    log(ERROR, "Unable to put database connection in non-blocking mode.");
                }
                int fd = PQsocket(con);
                if (fd >= 0) {
                    m_socket = new boost::asio::ip::tcp::socket(m_io_service);
                    m_socket->assign(boost::asio::ip::tcp::v4(), fd);
                    do_read();
                    return;
                }
            }
        }
        this->tryReConnect();
    });
}


CommPSQLSocket::~CommPSQLSocket()
{
    m_db.shutdownConnection();
    delete m_socket;
}

void CommPSQLSocket::do_read()
{
    //only use asio to poll for data available; use the PG* functions
    //to do the actual reading
    m_socket->async_read_some(boost::asio::null_buffers(),
            [this](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    int result = this->read();
                    if (result == 0) {
                        this->dispatch();
                        this->do_read();
                    } else {
                        delete m_socket;
                        m_socket = nullptr;
                        this->tryReConnect();
                    }
                } else {
                    delete m_socket;
                    m_socket = nullptr;
                    this->tryReConnect();
                }
            });
}

int CommPSQLSocket::read()
{
    debug(std::cout << "CommPSQLSocket::read()" << std::endl << std::flush;);
    PGconn * con = m_db.getConnection();
    assert(con != 0);

    if (PQstatus(con) != CONNECTION_OK) {
        log(ERROR, "Database connection closed.");
        return -1;
    }

    if (PQconsumeInput(con) == 0) {
        log(ERROR, "Error reading from database connection.");
        m_db.reportError();
        
        log(ERROR, "Connection to RDBMS lost.");
        return 1;
    }

    PGresult * res;
    while (PQisBusy(con) == 0) {
        if ((res = PQgetResult(con)) != 0) {
            m_db.queryResult(PQresultStatus(res));
            PQclear(res);
        } else {
            m_db.queryComplete();
            return 0;
        }
    };

    return 0;
}

void CommPSQLSocket::dispatch()
{
    debug(std::cout << "CommPSQLSocket::dispatch()"
                    << std::endl << std::flush;);

    if (m_db.queryInProgress()) {
        return;
    }

    m_db.launchNewQuery();
}

void CommPSQLSocket::vacuum()
{
    if (m_socket && m_socket->is_open()) {
        if (m_vacuumFull) {
            m_db.runMaintainance(Database::MAINTAIN_VACUUM |
                                 Database::MAINTAIN_VACUUM_FULL);
        } else {
            m_db.runMaintainance(Database::MAINTAIN_VACUUM |
                                 Database::MAINTAIN_VACUUM_ANALYZE);
        }
        m_vacuumFull = !m_vacuumFull;
    }

    m_vacuumTimer.expires_from_now(boost::posix_time::seconds(vacFreq));
    m_vacuumTimer.async_wait([this](boost::system::error_code ec) {
        if (!ec) {
            this->vacuum();
        }
    });
}


void CommPSQLSocket::reindex()
{
    if (m_socket && m_socket->is_open()) {
        m_db.runMaintainance(Database::MAINTAIN_REINDEX);
    }
    m_reindexTimer.expires_from_now(boost::posix_time::seconds(reindexFreq));
    m_reindexTimer.async_wait([this](boost::system::error_code ec) {
        if (!ec) {
            this->reindex();
        }
    });
}
