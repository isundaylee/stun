#include "event/Condition.h"
#include "event/EventLoop.h"

namespace event {

Condition::Condition(ConditionType type) :
    type(type),
    value(false) {
  EventLoop::getCurrentLoop()->addCondition(this);
}

Condition::~Condition() {
  EventLoop::getCurrentLoop()->removeCondition(this);
}

}
