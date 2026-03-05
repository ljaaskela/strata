#include "animation_track.h"

#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_property.h>

#include <algorithm>

namespace velk {

AnimationTrackImpl::~AnimationTrackImpl()
{
    if (transient_) {
        AnimationTrackImpl::uninstall();
    }
}

void AnimationTrackImpl::uninstall()
{
    for (auto& entry : targets_) {
        auto owner = entry.owner.lock();
        if (!owner) {
            continue;
        }
        auto* pi = interface_cast<IPropertyInternal>(owner);
        if (pi) {
            pi->remove_extension(get_self<IAnyExtension>());
        }
    }
    targets_.clear();
    display_ = nullptr;
    result_ = nullptr;
    interpolator_ = nullptr;
}

void AnimationTrackImpl::set_transient(bool transient)
{
    transient_ = transient;
}

bool AnimationTrackImpl::is_active() const
{
    auto* s = state();
    return s && s->state == PlayState::Playing;
}

const IAnimationTrack::State* AnimationTrackImpl::state() const
{
    return interface_state<IAnimationTrack>();
}

IAnimationTrack::State* AnimationTrackImpl::state()
{
    return interface_state<IAnimationTrack>();
}


void AnimationTrackImpl::set_keyframes(array_view<KeyframeEntry> keyframes)
{
    auto* s = state();
    if (!s) {
        return;
    }
    s->keyframes.clear();
    s->keyframes.insert(s->keyframes.begin(), keyframes.begin(), keyframes.end());
    sorted_ = false;
}

void AnimationTrackImpl::play()
{
    auto* s = state();
    if (!s) {
        return;
    }
    if (s->state == PlayState::Idle || s->state == PlayState::Finished) {
        s->elapsed = {};
        s->progress = 0.f;
    }
    s->state = PlayState::Playing;
    notify_state(*s);
}

void AnimationTrackImpl::pause()
{
    auto* s = state();
    if (!s || s->state != PlayState::Playing) {
        return;
    }
    s->state = PlayState::Paused;
    notify_state(*s);
}

void AnimationTrackImpl::stop()
{
    auto* s = state();
    if (!s || s->state == PlayState::Idle) {
        return;
    }
    s->elapsed = {};
    s->progress = 0.f;
    s->state = PlayState::Idle;
    notify_state(*s);
}

void AnimationTrackImpl::finish()
{
    auto* s = state();
    if (!s || s->state == PlayState::Finished) {
        return;
    }
    ensure_init(*s);
    mark_finished(*s);
}

void AnimationTrackImpl::restart()
{
    auto* s = state();
    if (!s) {
        return;
    }
    s->elapsed = {};
    s->progress = 0.f;
    s->state = PlayState::Playing;
    notify_state(*s);
}

void AnimationTrackImpl::seek(float p)
{
    auto* s = state();
    if (!s) {
        return;
    }
    ensure_init(*s);
    if (p < 0.f) {
        p = 0.f;
    }
    if (p > 1.f) {
        p = 1.f;
    }
    s->elapsed.us = static_cast<int64_t>(static_cast<float>(s->duration.us) * p);
    s->progress = p;
    apply_at(*s);
    notify_state(*s);
}

void AnimationTrackImpl::ensure_init(IAnimationTrack::State& s)
{
    if (sorted_) {
        return;
    }
    std::sort(s.keyframes.begin(), s.keyframes.end(), [](const KeyframeEntry& a, const KeyframeEntry& b) {
        return a.time.us < b.time.us;
    });
    if (!s.keyframes.empty()) {
        s.duration = s.keyframes.back().time;
    }

    // Resolve type info and interpolator from first target's inner
    if (!targets_.empty() && targets_[0].inner) {
        auto& inner = targets_[0].inner;
        auto types = inner->get_compatible_types();
        if (!types.empty()) {
            typeUid_ = types[0];
            interpolator_ = instance().type_registry().find_interpolator(typeUid_);
        }
        result_ = inner->clone();
    }
    sorted_ = true;
}

void AnimationTrackImpl::apply_at(IAnimationTrack::State& s)
{
    if (!has_targets()) {
        return;
    }
    if (s.keyframes.size() < 2) {
        if (!s.keyframes.empty() && s.keyframes.front().value) {
            write_value(*s.keyframes.front().value);
        }
        return;
    }
    if (s.elapsed.us <= 0) {
        if (s.keyframes.front().value) {
            write_value(*s.keyframes.front().value);
        }
        return;
    }
    if (s.elapsed.us >= s.duration.us) {
        if (s.keyframes.back().value) {
            write_value(*s.keyframes.back().value);
        }
        return;
    }

    // Find the segment: first keyframe with time > elapsed
    size_t i = 1;
    while (i < s.keyframes.size() && s.keyframes[i].time.us <= s.elapsed.us) {
        ++i;
    }
    if (i >= s.keyframes.size() || !interpolator_ || !result_) {
        return;
    }

    auto& kf0 = s.keyframes[i - 1];
    auto& kf1 = s.keyframes[i];
    if (kf0.value && kf1.value) {
        int64_t seg_len = kf1.time.us - kf0.time.us;
        float seg_t = (seg_len > 0)
                          ? static_cast<float>(s.elapsed.us - kf0.time.us) / static_cast<float>(seg_len)
                          : 1.f;
        interpolator_(*kf0.value, *kf1.value, kf1.easing(seg_t), *result_);
        write_value(*result_);
    }
}

void AnimationTrackImpl::mark_finished(IAnimationTrack::State& s)
{
    s.elapsed = s.duration;
    s.progress = 1.f;
    s.state = PlayState::Finished;
    apply_at(s);
    notify_state(s);
}

void AnimationTrackImpl::write_value(const IAny& value)
{
    if (display_) {
        display_->copy_from(value);
    }
    for (auto& entry : targets_) {
        if (entry.inner) {
            entry.inner->copy_from(value);
        }
        auto prop = entry.owner.lock();
        if (prop) {
            auto pi = interface_pointer_cast<IPropertyInternal>(prop);
            if (pi) {
                instance().queue_deferred_property({pi, nullptr});
            }
        }
    }
}

void AnimationTrackImpl::notify_state(IAnimationTrack::State& state)
{
    notify(MemberKind::Property, IAnimationTrack::UID, Notification::Changed);
}

ReturnValue AnimationTrackImpl::tick(const UpdateInfo& info)
{
    // Skip if not actively playing
    auto* st = state();
    if (!st || st->state != PlayState::Playing) {
        return ReturnValue::NothingToDo;
    }
    auto& s = *st;

    // Degenerate: 0 or 1 keyframes, apply and finish immediately
    if (s.keyframes.size() < 2) {
        mark_finished(s);
        return ReturnValue::Success;
    }

    // Sort keyframes on first tick, resolve interpolator
    ensure_init(s);
    s.elapsed.us += info.dt.us;

    // Reached the end: snap to final keyframe
    if (s.elapsed.us >= s.duration.us) {
        mark_finished(s);
        return ReturnValue::Success;
    }

    // Mid-animation: interpolate at current elapsed time
    s.progress =
        (s.duration.us > 0) ? static_cast<float>(s.elapsed.us) / static_cast<float>(s.duration.us) : 0.f;
    apply_at(s);
    notify_state(s);
    return ReturnValue::Success;
}

// IAnyExtension

IAny::ConstPtr AnimationTrackImpl::get_inner() const
{
    return targets_.empty() ? nullptr : targets_[0].inner;
}

void AnimationTrackImpl::set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner)
{
    // First target entry: initialize display/result/interpolator
    if (targets_.empty() && inner) {
        display_ = inner->clone();
        result_ = inner->clone();

        auto types = inner->get_compatible_types();
        if (!types.empty()) {
            typeUid_ = types[0];
            interpolator_ = instance().type_registry().find_interpolator(typeUid_);
        }
    }
    targets_.push_back({owner, std::move(inner)});
}

IAny::Ptr AnimationTrackImpl::take_inner(IInterface& owner)
{
    for (auto it = targets_.begin(); it != targets_.end(); ++it) {
        auto locked = it->owner.lock();
        if (locked && locked.get() == &owner) {
            auto inner = std::move(it->inner);
            targets_.erase(it);
            if (targets_.empty()) {
                display_ = nullptr;
                result_ = nullptr;
                interpolator_ = nullptr;
            }
            return inner;
        }
    }
    return nullptr;
}

// IAnimationTrack: add_target / remove_target

void AnimationTrackImpl::add_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    if (pi) {
        pi->install_extension(get_self<IAnyExtension>());
    }
}

void AnimationTrackImpl::remove_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    if (pi) {
        pi->remove_extension(get_self<IAnyExtension>());
    }
}

// IAny overrides

array_view<Uid> AnimationTrackImpl::get_compatible_types() const
{
    return display_ ? display_->get_compatible_types() : array_view<Uid>{};
}

size_t AnimationTrackImpl::get_data_size(Uid type) const
{
    return display_ ? display_->get_data_size(type) : 0;
}

ReturnValue AnimationTrackImpl::get_data(void* to, size_t toSize, Uid type) const
{
    auto* s = state();
    if (s && (s->state == PlayState::Playing || s->state == PlayState::Paused)) {
        return display_ ? display_->get_data(to, toSize, type) : ReturnValue::Fail;
    }
    return (!targets_.empty() && targets_[0].inner) ? targets_[0].inner->get_data(to, toSize, type)
                                                    : ReturnValue::Fail;
}

ReturnValue AnimationTrackImpl::set_data(void const* from, size_t fromSize, Uid type)
{
    ReturnValue ret = ReturnValue::Fail;
    for (auto& entry : targets_) {
        if (entry.inner) {
            auto r = entry.inner->set_data(from, fromSize, type);
            if (succeeded(r)) {
                ret = r;
            }
        }
    }
    if (display_ && succeeded(ret)) {
        display_->set_data(from, fromSize, type);
    }
    return ret;
}

ReturnValue AnimationTrackImpl::copy_from(const IAny& other)
{
    ReturnValue ret = ReturnValue::Fail;
    for (auto& entry : targets_) {
        if (entry.inner) {
            auto r = entry.inner->copy_from(other);
            if (succeeded(r)) {
                ret = r;
            }
        }
    }
    if (display_ && succeeded(ret)) {
        display_->copy_from(other);
    }
    return ret;
}

IAny::Ptr AnimationTrackImpl::clone() const
{
    return display_ ? display_->clone()
                    : ((!targets_.empty() && targets_[0].inner) ? targets_[0].inner->clone() : nullptr);
}

} // namespace velk
