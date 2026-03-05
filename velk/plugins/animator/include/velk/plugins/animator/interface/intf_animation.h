#ifndef VELK_ANIMATOR_INTF_ANIMATION_H
#define VELK_ANIMATOR_INTF_ANIMATION_H

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_velk.h>

namespace velk {

/**
 * @brief Base interface for all animations (transitions and keyframe tracks).
 *
 * Provides the common contract: tick, target management, uninstall, transient lifecycle.
 */
class IAnimation : public Interface<IAnimation>
{
public:
    /** @brief Advances the animation. Called by the animator; not for external use. */
    virtual ReturnValue tick(const UpdateInfo& info) = 0;
    /** @brief Installs this animation on a property target. */
    virtual void add_target(const IProperty::Ptr& target) = 0;
    /** @brief Removes this animation from a property target. */
    virtual void remove_target(const IProperty::Ptr& target) = 0;
    /** @brief Detaches the animation from all property targets. */
    virtual void uninstall() = 0;
    /** @brief When true, dropping the last handle calls uninstall(). Default: false (persistent). */
    virtual void set_transient(bool transient) = 0;
    /** @brief Returns true if this animation is currently active (playing or animating). */
    virtual bool is_active() const = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATION_H
