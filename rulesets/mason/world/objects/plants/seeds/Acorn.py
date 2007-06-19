from atlas import *

from cyphesis.Thing import Thing
from world.objects.plants.seeds.Seed import Seed

import atlas


class Acorn(Seed):
    def eat_operation(self, op):
        ent=Entity(self.id,status=-1)
        res = Operation("set",ent,to=self)
        to_ = op.from_
        nour=Entity(to_,mass=self.mass)
        res = res + Operation("nourish",nour,to=to_)
        return res
    def setup_operation(self, op):
        # Decay a bit, we don't last forever
        # Of course, a real acorn would germinate after a bit
        opTick=Operation("tick",to=self)
        opTick.setFutureSeconds(1800)
        return opTick
    def tick_operation(self, op):
        ent=Entity(self.id,status=-1)
        return Operation("set",ent,to=self)
    def germinate_operation(self, op):
        set_ent=Entity(self.id,status=-1)
        create_ent=Entity(name='oak',parents=['oak'],location=Location(self.location.parent, self.location.coordinates),mass=self.mass,bbox=[-0.02, -0.02, 0, 0.02, 0.02, 0.12])
        res = Operation("set",set_ent,to=self)
        res = res + Operation("create",create_ent,to=self)
        return res
