#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 1999 Aloril (See the file COPYING for details).
from atlas import *

from cyphesis.Thing import Thing
from misc import set_kw
from whrandom import *
from Vector3D import Vector3D

import atlas

class Pig(Thing):
    def __init__(self, cppthing, **kw):
        self.base_init(cppthing, kw)
        set_kw(self,kw,"mass",5.0)
        self.maxmass=100.0
    def chop_operation(self, op):
        if self.mass<1:
            return(Operation("set",Entity(self.id,status=-1),to=self))
        res = Oplist()
        ent=Entity(self.id,mode="dead",mass=self.mass-1)
        res.append(Operation("set",ent,to=self))
        ham_ent=Entity(name='ham',parent='ham',location=self.location.copy())
        if (len(op)>1):
            to_ = op[1].id
        else:
            to_=self
        res.append(Operation("create",ham_ent,to=to_))
        return res
