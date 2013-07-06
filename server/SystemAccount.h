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


#ifndef SERVER_SYSTEM_ACCOUNT_H
#define SERVER_SYSTEM_ACCOUNT_H

#include "server/Admin.h"

class SystemAccount : public Admin {
  public:
    SystemAccount(Connection * conn,
                  const std::string & username,
                  const std::string & passwd,
                  const std::string & id, long intId);
    ~SystemAccount();

    virtual const char * getType() const;
    virtual bool isPersisted() const;
};


#endif // SERVER_SYSTEM_ACCOUNT_H
