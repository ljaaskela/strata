#ifndef VELK_ANIMATOR_INTF_TRANSITION_H
#define VELK_ANIMATOR_INTF_TRANSITION_H

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_property.h>
#include <velk/plugins/animator/easing.h>
#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

/**
 * @brief Interface for an implicit property transition.
 *
 * Inherits IAnimation (tick, add_target, remove_target, uninstall, set_transient).
 *
 * Properties (via VELK_INTERFACE):
 *   - duration  (Duration): Transition duration (read/write).
 *   - animating (bool):     Whether the transition is currently animating (read-only).
 */
class ITransition : public Interface<ITransition, IAnimation>
{
public:
    VELK_INTERFACE(
        (PROP, Duration, duration, {}),
        (PROP, bool,     animating, false)
    )

    /** @brief Sets the easing function. */
    virtual void set_easing(easing::EasingFn easing) = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_TRANSITION_H
