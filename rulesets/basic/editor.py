#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 1999 Aloril (See the file COPYING for details).
from atlas import *
from types import *
from mind.panlingua import interlinguish
il=interlinguish

import server

class editor:
    def __init__(self, client, avatar):
        self.client=client
        self.avatar=avatar
        self.list_call={"say":(editor._say,1),
                        "own":(editor._own,2),
                        "know":(editor._know,2),
                        "learn":(editor._learn,2),
                        "tell":(editor._tell,1)}
    def __del__(self):
        self.client.teardown()
    def call_list_args(self, *args):
        #indent='\t'*self.cl_depth
        for i in range(self.cl_args[1]):
            if type(args[i])==ListType:
                #print indent,"LIST!"
                #print indent,self.cl_args,args
                #self.cl_depth=self.cl_depth+1
                for a in args[i]:
                    self.call_list_args(*args[:i]+(a,)+args[i+1:])
                #self.cl_depth=self.cl_depth-1
                return
        #print indent,"SINGLE!"
        #print indent,self.cl_args,args
        self.cl_args[0](self, *args)
    def __getattr__(self, name):
        if self.list_call.has_key(name):
            self.cl_args=self.list_call[name]
            #self.cl_depth=0
            return self.call_list_args
        raise AttributeError,name
    def create(self, avatar_type):
        return self.client.create_avatar(avatar_type)
    def make(self, type, **kw):
        kw['type']=type
        # if not kw.has_key('type'):
            # kw['type']=name
        ent=apply(Entity,(),kw)
        #ent=Entity(kw)
##         if hasattr(ent,"copy"):
##             foo
        return self.avatar.make(ent)
    def set(self, _id, **kw):
        kw['id']=_id
        ent=apply(Entity,(),kw)
        return self.avatar.set(_id,ent)
    def look(self, _id=""):
        return self.avatar.look(_id)
    def look_for(self, **kw):
        ent=apply(Entity,(),kw)
        return self.avatar.look_for(ent)
    def delete(self, id):
        return self.avatar.delete(id)
    def sanitizeKnowledge(self, knowledge):
        if type(knowledge)==InstanceType: return knowledge.id
        elif type(knowledge)==TupleType: return `knowledge`
        elif type(knowledge)==StringType: return knowledge
        else: return knowledge.id
    
    def _say(self,target,verb,subject,object,predicate=None):
##         es=Entity(verb=verb,subject=subject,object=object)
##         self.avatar.send(Operation("talk",es,to=target))
        if type(subject)==InstanceType: subject=subject.id
        elif type(subject)==TupleType: subject=`subject`
        elif type(subject)==StringType: pass
        else: subject=subject.id
        if type(object)==InstanceType: object=object.id
        elif type(object)==TupleType: object=`object`
        elif type(object)==StringType: pass
        else: object=object.id
        if predicate:
            string,interlinguish=il.verb_subject_predicate_object(verb,subject,predicate,object)
        else:
            string,interlinguish=il.verb_subject_object(verb,subject,object)
        self._tell(target,string,interlinguish)
    def _own(self,target,object):
        self._say(target,'own',target,object)
    def _know(self,target,know):
        subject = know[0]
        
        if len(know)==2:
            predicate = 'location'
            object = know[1]
        else:
            predicate = know[1]
            object = know[2]
        
        es=Entity(subject=self.sanitizeKnowledge(subject), object=self.sanitizeKnowledge(object), predicate=predicate)

        set=Operation("set")
        set.setArgs([es])
        think=Operation("think", to=target)
        think.setArgs([set])
        self.avatar.send(think)
    def _learn(self,target,goal):
        es=Entity(id=goal[1], goal=goal[1])
        set=Operation("set")
        set.setArgs([es])
        think=Operation("think", to=target)
        think.setArgs([set])
        
        self.avatar.send(think)
    #Interlinguish
    def _tell(self,target,string,interlinguish):
        es=Entity(say=string)
        self.avatar.send(Operation("talk",es,to=target))
    def tell_importance(self,target,sub,cmp,obj):
        s,i=il.importance(sub,cmp,obj)
        self.tell(target,s,i)

def create_editor(host, account, password, avatar='creator'):
    client=server.ObserverClient()

    client.server = host

    client.setup(account, password, avatar)

    avatar = client.create_avatar(avatar)

    return editor(client, avatar)
