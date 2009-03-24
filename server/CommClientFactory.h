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

// $Id: 17bdaf1cd59b6cf3e3a881f4d2823f8168bf6bbe $

#include <string>

class CommServer;
class CommClient;

/// \brief Kit class for factories to create CommClient objects.
class CommClientKit {
  public:
    virtual ~CommClientKit();

    virtual int newCommClient(CommServer &, int, const std::string &) = 0;
};

template <class ConnectionT>
class CommClientFactory : public CommClientKit {
  public:
    virtual int newCommClient(CommServer &, int, const std::string &);
};

class CommHttpClientFactory : public CommClientKit {
  public:
    virtual int newCommClient(CommServer &, int, const std::string &);
};

class CommPythonClientFactory : public CommClientKit {
  public:
    virtual int newCommClient(CommServer &, int, const std::string &);
};
