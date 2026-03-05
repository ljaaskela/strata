#ifndef VELK_ANIMATOR_INTF_ANIMATOR_H
#define VELK_ANIMATOR_INTF_ANIMATOR_H

#include <velk/interface/intf_metadata.h>
#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

/**
 * @brief Interface for an animator that manages and ticks a set of animations.
 *
 * Animations are added via add() and advanced each frame via tick().
 * The animator holds weak references; animations persist via property installation.
 */
class IAnimator : public Interface<IAnimator>
{
public:
    /** @brief Advances all animations. */
    virtual void tick(const UpdateInfo& info) = 0;
    /** @brief Adds an animation to be managed. */
    virtual void add(const IAnimation::Ptr& animation) = 0;
    /** @brief Removes an animation from the animator. */
    virtual void remove(const IAnimation::Ptr& animation) = 0;
    /** @brief Stops all animations and removes them. */
    virtual void cancel_all() = 0;
    /** @brief Returns the number of currently active animations. */
    virtual size_t active_count() const = 0;
    /** @brief Returns the total number of managed animations (excluding expired). */
    virtual size_t count() const = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATOR_H
