#include "animator.h"

#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

void AnimatorImpl::tick(const UpdateInfo& info)
{
    size_t write = 0;
    for (size_t i = 0; i < animations_.size(); ++i) {
        auto anim = animations_[i].lock();
        if (!anim) {
            continue;
        }
        anim->tick(info);
        animations_[write++] = animations_[i];
    }
    animations_.resize(write); // Removes any IAnimation::WeakPtrs whose .lock() failed above.
}

void AnimatorImpl::add(const IAnimation::Ptr& animation)
{
    if (!animation) {
        return;
    }
    for (auto& weak : animations_) {
        auto locked = weak.lock();
        if (locked && locked.get() == animation.get()) {
            return;
        }
    }
    animations_.push_back(IAnimation::WeakPtr(animation));
}

void AnimatorImpl::remove(const IAnimation::Ptr& animation)
{
    if (!animation) {
        return;
    }
    for (size_t i = 0; i < animations_.size(); ++i) {
        auto locked = animations_[i].lock();
        if (locked && locked.get() == animation.get()) {
            animations_.erase(animations_.begin() + static_cast<ptrdiff_t>(i));
            return;
        }
    }
}

void AnimatorImpl::cancel_all()
{
    for (auto& weak : animations_) {
        auto anim = weak.lock();
        if (anim) {
            anim->uninstall();
        }
    }
    animations_.clear();
}

size_t AnimatorImpl::active_count() const
{
    size_t n = 0;
    for (auto& weak : animations_) {
        auto anim = weak.lock();
        if (anim && anim->is_active()) {
            ++n;
        }
    }
    return n;
}

size_t AnimatorImpl::count() const
{
    size_t n = 0;
    for (auto& weak : animations_) {
        if (weak.lock()) {
            ++n;
        }
    }
    return n;
}

} // namespace velk
