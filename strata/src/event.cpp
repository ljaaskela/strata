#include "event.h"

namespace strata {

ReturnValue EventImpl::invoke(const IAny *args) const
{
    for (const auto &h : handlers_) {
        h->invoke(args);
    }
    return ReturnValue::SUCCESS;
}

const IFunction::ConstPtr EventImpl::get_invocable() const
{
    return get_self<IFunction>();
}

ReturnValue EventImpl::add_handler(const IFunction::ConstPtr &fn) const
{
    if (!fn) {
        return ReturnValue::INVALID_ARGUMENT;
    }
    for (const auto &h : handlers_) {
        if (h == fn) {
            return ReturnValue::NOTHING_TO_DO;
        }
    }
    handlers_.push_back(fn);
    return ReturnValue::SUCCESS;
}

ReturnValue EventImpl::remove_handler(const IFunction::ConstPtr &fn) const
{
    auto pos = 0;
    for (const auto &h : handlers_) {
        if (h == fn) {
            handlers_.erase(handlers_.begin() + pos);
            return ReturnValue::SUCCESS;
        }
        pos++;
    }
    return ReturnValue::NOTHING_TO_DO;
}

} // namespace strata
