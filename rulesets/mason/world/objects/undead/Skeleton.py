#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 1999 Al Riddoch (See the file COPYING for details).

from atlas import *
from common import log,const
from physics import Vector3D
from random import *

import server

class Skeleton(server.Thing):
    def touch_operation(self, op):
        retops = Oplist()
        if self.status<0: return
        self.status=-1
        newloc=self.location.copy()
        newloc.velocity=Vector3D()
        retops = retops + Operation("move", Entity(self.id, location=newloc.copy(), mode="collapsed"), to=self)
        for item in ['skull', 'ribcage', 'arm', 'pelvis', 'thigh', 'shin']:
            newloc.coordinates = newloc.coordinates + Vector3D(uniform(-1,1), uniform(-1,1), uniform(-1,1))
            retops = retops + Operation("create", Entity(name=item,parent=item,location=newloc.copy()), to=self)
        retops = retops + Operation("set", Entity(self.id, status=self.status), to=self)
        return retops
