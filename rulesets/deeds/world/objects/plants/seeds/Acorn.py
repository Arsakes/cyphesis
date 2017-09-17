from atlas import *

from world.objects.plants.seeds.Seed import Seed

class Acorn(Seed):
    """An acorn which germinates."""
    def germinate_operation(self, op):
        set_ent=Entity(self.id,status=-1)
        create_ent=Entity(name='oak',parent='oak',location=Location(self.location.parent, self.location.coordinates),mass=self.mass,bbox=[-0.02, -0.02, 0, 0.02, 0.02, 0.12])
        res = Operation("set",set_ent,to=self)
        res = res + Operation("create",create_ent,to=self)
        return res
