#ifndef VELK_API_CHANGE_H
#define VELK_API_CHANGE_H

namespace velk {

/**
 * @brief Tracks whether a value has changed since the last observation.
 *
 * Small utility for "recompute only when inputs differ" patterns. Bundle the
 * inputs that drive a computation into a POD `Key` struct (with `operator==`),
 * then call `changed(key)` once per cycle. It returns `true` the first time and
 * whenever the key differs from the previously observed value, and `false`
 * otherwise. Typical use: early-return from an expensive function when nothing
 * relevant has changed.
 *
 * The first call always returns `true` (no prior value to compare against).
 * Use `invalidate()` to force the next call to return `true` regardless.
 *
 * Comparison is exact equality on the `Key`, not a hash, so there are no
 * false negatives from collisions. The `Key` should be cheap to copy and
 * compare; small PODs are ideal.
 *
 * Example:
 * @code
 * struct Key { int a; float b; bool operator==(const Key& o) const { return a == o.a && b == o.b; } };
 * ChangeCache<Key> cache;
 *
 * void update() {
 *     Key k{current_a(), current_b()};
 *     if (!cache.changed(k)) return; // inputs unchanged, skip work
 *     // ... expensive recompute ...
 * }
 * @endcode
 *
 * @tparam Key The type of value to track. Must be default-constructible,
 *             copy-assignable, and equality-comparable.
 */
template <class Key>
class ChangeCache
{
public:
    /**
     * @brief Compare @p key against the last observed value and update the store.
     * @return `true` if @p key differs from the previous value (or this is the
     *         first call since construction / `invalidate()`), `false` otherwise.
     */
    bool changed(const Key& key)
    {
        if (valid_ && key == last_) {
            return false;
        }
        last_ = key;
        valid_ = true;
        return true;
    }

    /// @brief Forget the stored value so the next `changed()` call returns `true`.
    void invalidate() { valid_ = false; }

private:
    Key last_{};
    bool valid_{false};
};

} // namespace velk

#endif // VELK_API_CHANGE_H
