// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2001 Alistair Riddoch

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/accountbase.h"
#include "common/globals.h"

#include <string>
#include <iostream>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#include <unistd.h>
#endif

// This is the currently very basic password management tool, which can
// be used to control the passwords of accounts in the main servers account
// database. This is the only way to create accounts on a server in
// restricted mode.

// TODO: Make sure the rest of the Object is preserved, rather than just
//       blatting it with a new Object.

using Atlas::Message::Element;

#define ADD 0
#define SET 1
#define DEL 2

void usage(char * n)
{
    std::cout << "usage: " << n << " -[asd] account" << std::endl << std::flush;
}

int main(int argc, char ** argv)
{
    if (loadConfig(argc, argv)) {
        // Fatal error loading config file
        return 1;
    }

    std::string acname;
    int action;

    if (argc == 1) {
        acname = "admin";
        action = SET;
    } else if (argc == 3) {
        if (argv[1][0] == '-') {
            int c = argv[1][1];
            if (c == 'a') {
                action = ADD;
            } else if (c == 's') {
                action = SET;
            } else if (c == 'd') {
                action = DEL;
            } else {
                usage(argv[0]);
                return 1;
            }
            acname = argv[2];
        } else {
            usage(argv[0]);
            return 1;
        }
    } else {
        usage(argv[0]);
        return 1;
    }

    AccountBase * db = AccountBase::instance(true);

    Element::MapType data;

    bool res = db->getAccount("admin", data);

    if (!res) {
        std::cout << "Admin account does not yet exist" << std::endl << std::flush;
        acname = "admin";
        action = ADD;
    }
    if (action != ADD) {
        Element::MapType o;
        res = db->getAccount(acname, o);
        if (!res) {
            std::cout<<"Account "<<acname<<" does not yet exist"<<std::endl<<std::flush;
            AccountBase::del();
            return 0;
        }
    }
    if (action == DEL) {
        db->delAccount(acname);
        if (res) {
            std::cout << "Account " << acname << " removed." << std::endl << std::flush;
        }
        AccountBase::del();
        return 0;
    }

#ifdef HAVE_TERMIOS_H
    termios termios_old, termios_new;
    
    tcgetattr( STDIN_FILENO, &termios_old );
    termios_new = termios_old;
    termios_new.c_lflag &= ~(ICANON|ECHO);
    tcsetattr( STDIN_FILENO, TCSADRAIN, &termios_new );
#endif
    
    std::string password, password2;
    std::cout << "New " << acname << " password: " << std::flush;
    std::cin >> password;
    std::cout << std::endl << "Retype " << acname << " password: " << std::flush;
    std::cin >> password2;
    std::cout << std::endl << std::flush;
    
#ifdef HAVE_TERMIOS_H
    tcsetattr( STDIN_FILENO, TCSADRAIN, &termios_old );
#endif
    
    if (password == password2) {
        Element::MapType amap;
        amap["id"] = acname;
        amap["password"] = password;

        if (action == ADD) {
            res = db->putAccount(amap, acname);
        } else {
            res = db->modAccount(amap, acname);
        }
        if (res) {
            std::cout << "Password changed." << std::endl << std::flush;
        }
    } else {
        std::cout << "Passwords did not match. Account database unchanged."
                  << std::endl << std::flush;
    }
    AccountBase::del();
}
