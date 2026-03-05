#ifndef VELK_ANIMATOR_INTF_ANIMATION_TRACK_H
#define VELK_ANIMATOR_INTF_ANIMATION_TRACK_H

#include <velk/array_view.h>
#include <velk/plugins/animator/easing.h>
#include <velk/plugins/animator/interface/intf_animation.h>

#include <cstdint>

namespace velk {

/** @brief Playback state for an animation track. */
enum class PlayState : uint8_t
{
    Idle = 0,    ///< Created but not yet played, or reset via stop().
    Playing = 1, ///< Actively advancing.
    Paused = 2,  ///< Frozen mid-playback, can resume.
    Finished = 3 ///< Reached the end naturally or via finish().
};

/** @brief A type-erased keyframe: time, value, and easing function. */
struct KeyframeEntry
{
    Duration time;
    IAny::Ptr value;
    easing::EasingFn easing = easing::linear;

    bool operator==(const KeyframeEntry& other) const
    {
        return time.us == other.time.us && easing == other.easing && value == other.value;
    }
    bool operator!=(const KeyframeEntry& other) const { return !(*this == other); }
};

/**
 * @brief Interface for a keyframe-based animation track.
 *
 * Inherits IAnimation (tick, add_target, remove_target, uninstall, set_transient).
 *
 * Properties (via VELK_INTERFACE):
 *   - duration  (Duration):  Total duration.
 *   - elapsed   (Duration):  Elapsed time.
 *   - progress  (float):     Normalized progress (0..1).
 *   - state     (PlayState): Current playback state.
 *   - keyframes (KeyframeEntry[]): Read-only keyframe array.
 */
class IAnimationTrack : public Interface<IAnimationTrack, IAnimation>
{
public:
    VELK_INTERFACE(
        (RPROP, Duration,       duration,  {}),
        (RPROP, Duration,       elapsed,   {}),
        (RPROP, float,          progress,  0.f),
        (RPROP, PlayState,      state,     PlayState::Idle),
        (RARR,  KeyframeEntry,  keyframes)
    )

    /** @brief Starts or resumes playback. */
    virtual void play() = 0;
    /** @brief Pauses playback, preserving position. */
    virtual void pause() = 0;
    /** @brief Stops playback and resets to the beginning (Idle). */
    virtual void stop() = 0;
    /** @brief Jumps to the end, applies the final value, and transitions to Finished. */
    virtual void finish() = 0;
    /** @brief Resets to beginning and starts playback. */
    virtual void restart() = 0;
    /** @brief Seeks to a normalized position (0..1) and applies the interpolated value. */
    virtual void seek(float progress) = 0;
    /** @brief Replaces all keyframes. Shares ownership of each entry's value. */
    virtual void set_keyframes(array_view<KeyframeEntry> keyframes) = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATION_TRACK_H
