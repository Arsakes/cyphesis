#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 2005 Al Riddoch (See the file COPYING for details).

from atlas import *

from physics import Point3D

import server

class Tinderbox(server.Thing):
    """This is base class for axes, this one just ordinary axe"""
    def ignite_operation(self, op):
        to_ = op[0].id
        if not to_:
            return self.error(op,"To is undefined object")
        return Operation("create",Entity(parent='fire',status=0.05, location=Location(server.world.get_object(to_),Point3D(0.0,0.0,0.0))),to=to_)
