/*
 Copyright (C) 2014 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H

#endif

#include "OperationsDispatcher.h"
#include "rulesets/LocatedEntity.h"
#include "BaseWorld.h"
#include "const.h"
#include "debug.h"
#include "Monitors.h"

#include <iostream>

static const bool debug_flag = false;

OpQueEntry::OpQueEntry(const Operation & o, LocatedEntity & f) : op(o),
                                                                 from(&f)
{
    from->incRef();
}

OpQueEntry::OpQueEntry(const OpQueEntry & o) : op(o.op), from(o.from)
{
    from->incRef();
}

OpQueEntry::~OpQueEntry()
{
    from->decRef();
}


OperationsDispatcher::OperationsDispatcher(const std::function<void(const Operation &, LocatedEntity &)> & operationProcessor, const std::function<double()> & timeProviderFn)
    : m_operationProcessor(operationProcessor), m_timeProviderFn(timeProviderFn), m_operation_queues_dirty(false)
{
}

OperationsDispatcher::~OperationsDispatcher()
{
    clearQueues();
}

void OperationsDispatcher::clearQueues()
{
    m_operationQueue = OpPriorityQueue();
}

void OperationsDispatcher::dispatchOperation(const OpQueEntry & oqe)
{
    //Set the time of when this op is dispatched. That way, other components in the system can
    //always use the seconds set on the op to know the current time.
    oqe.op->setSeconds(getTime());
    try {
        m_operationProcessor(oqe.op, *oqe.from);
    }
    catch (const std::exception & ex) {
        log(ERROR, String::compose("Exception caught in WorldRouter::idle() "
                                       "thrown while processing operation "
                                       "sent to \"%1\" from \"%2\": %3",
                                   oqe->getTo(), oqe->getFrom(), ex.what()));
    }
    catch (...) {
        log(ERROR, String::compose("Unspecified exception caught in WorldRouter::idle() "
                                       "thrown while processing operation "
                                       "sent to \"%1\" from \"%2\"",
                                   oqe->getTo(), oqe->getFrom()));
    }
}

/// \brief Add an operation to the ordered op queue.
///
/// Any time adjustment required is made to the operation, and it
/// is added to the appropriate place in the chronologically ordered
/// queue. The From attribute of the operation is set to the id of
/// the entity that is responsible for adding the operation to the
/// queue.
void OperationsDispatcher::addOperationToQueue(const Operation & op, LocatedEntity & ent)
{
    assert(op.isValid());
    assert(op->getFrom() != "cheat");

    m_operation_queues_dirty = true;
    op->setFrom(ent.getId());
    if (!op->hasAttrFlag(Atlas::Objects::Operation::SECONDS_FLAG)) {
        if (!op->hasAttrFlag(Atlas::Objects::Operation::FUTURE_SECONDS_FLAG)) {
            op->setSeconds(getTime());
        } else {
            double t = getTime() + (op->getFutureSeconds() * consts::time_multiplier);
            op->setSeconds(t);
            op->removeAttrFlag(Atlas::Objects::Operation::FUTURE_SECONDS_FLAG);
        }
    }
    m_operationQueue.push(OpQueEntry(op, ent));
    if (debug_flag) {
        std::cout << "WorldRouter::addOperationToQueue {" << std::endl;
        debug_dump(op, std::cout);
        std::cout << "}" << std::endl << std::flush;
    }
}

bool OperationsDispatcher::idle()
{
    unsigned int op_count = 0;

    double realtime = getTime();
    bool opsAvailableRightNow = !m_operationQueue.empty() && m_operationQueue.top()->getSeconds() <= realtime;

    while (opsAvailableRightNow && op_count < 10) {
        ++op_count;
        auto opQueueEntry = std::move(m_operationQueue.top());
        //Pop it before we dispatch it, since dispatching might alter the queue.
        m_operationQueue.pop();
        dispatchOperation(opQueueEntry);

        opsAvailableRightNow = !m_operationQueue.empty() && m_operationQueue.top()->getSeconds() <= realtime;
    };
    // If there are still ops to deliver return true
    // to tell the server not to sleep when polling clients. This ensures
    // that we keep processing ops at a the maximum rate without leaving
    // clients unattended.
    Monitors::instance()->insert("operations_queue", (Atlas::Message::IntType) m_operationQueue.size());
    return opsAvailableRightNow;
}


bool OperationsDispatcher::isQueueDirty() const
{
    return m_operation_queues_dirty;
}

void OperationsDispatcher::markQueueAsClean()
{
    m_operation_queues_dirty = false;
}

double OperationsDispatcher::getTime() const
{
    return m_timeProviderFn();
}

double OperationsDispatcher::secondsUntilNextOp() const
{
    if (m_operationQueue.empty()) {
        //600 is a fairly large number of seconds
        return 600.0;
    }
    return m_operationQueue.top()->getSeconds() - getTime();
}

