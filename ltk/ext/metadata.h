#ifndef EXT_METADATA_H
#define EXT_METADATA_H

#include <api/ltk.h>
#include <array_view.h>
#include <interface/intf_metadata.h>
#include <interface/types.h>

#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// -- Constexpr metadata collection from interfaces --

/** @brief Gets T::metadata if it exists, otherwise an empty array. */
template<class T, class = void>
struct type_metadata {
    static constexpr std::array<MemberDesc, 0> value{};
};

template<class T>
struct type_metadata<T, std::void_t<decltype(T::metadata)>> {
    static constexpr auto value = T::metadata;
};

/** @brief Constexpr concatenation of two std::arrays. */
template<class T, size_t N1, size_t N2>
constexpr std::array<T, N1 + N2> concat_arrays(std::array<T, N1> a, std::array<T, N2> b)
{
    std::array<T, N1 + N2> result{};
    for (size_t i = 0; i < N1; ++i) result[i] = a[i];
    for (size_t i = 0; i < N2; ++i) result[N1 + i] = b[i];
    return result;
}

/** @brief Collects metadata arrays from all interfaces into a single array. */
template<class...>
struct collected_metadata;

template<>
struct collected_metadata<> {
    static constexpr std::array<MemberDesc, 0> value{};
};

template<class First, class... Rest>
struct collected_metadata<First, Rest...> {
    static constexpr auto value = concat_arrays(
        type_metadata<First>::value,
        collected_metadata<Rest...>::value
    );
};

// -- MetaDataMixin --

/**
 * @brief Private mixin providing IMetaData implementation for MetaObject.
 *
 * Runtime instances (properties, events, functions) are created lazily
 * on first access via the getter methods, using the static metadata table
 * from FinalClass::metadata to find the matching descriptor.
 *
 * @tparam FinalClass The concrete CRTP class (must have a static metadata member).
 */
template<class FinalClass>
class MetaDataMixin
{
protected:
    MetaDataMixin() = default;

    /** @brief Returns static metadata as an array_view over FinalClass::metadata. */
    array_view<MemberDesc> GetMembersImpl() const
    {
        return {FinalClass::metadata.data(), FinalClass::metadata.size()};
    }

    /** @brief Looks up (or lazily creates) a runtime property by name. */
    IProperty::Ptr GetPropertyImpl(std::string_view name) const
    {
        for (auto& [n, p] : properties_) {
            if (n == name) return p;
        }
        for (auto& desc : FinalClass::metadata) {
            if (desc.kind == MemberKind::Property && desc.name == name) {
                if (auto p = GetRegistry().CreateProperty(desc.typeUid, {})) {
                    properties_.push_back({name, p});
                    return p;
                }
            }
        }
        return {};
    }

    /** @brief Looks up (or lazily creates) a runtime event by name. */
    IEvent::Ptr GetEventImpl(std::string_view name) const
    {
        for (auto& [n, e] : events_) {
            if (n == name) return e;
        }
        for (auto& desc : FinalClass::metadata) {
            if (desc.kind == MemberKind::Event && desc.name == name) {
                if (auto e = GetRegistry().template Create<IEvent>(ClassId::Event)) {
                    events_.push_back({name, e});
                    return e;
                }
            }
        }
        return {};
    }

    /** @brief Looks up (or lazily creates) a runtime function by name. */
    IFunction::Ptr GetFunctionImpl(std::string_view name) const
    {
        for (auto& [n, f] : functions_) {
            if (n == name) return f;
        }
        for (auto& desc : FinalClass::metadata) {
            if (desc.kind == MemberKind::Function && desc.name == name) {
                if (auto f = GetRegistry().template Create<IFunction>(ClassId::Function)) {
                    functions_.push_back({name, f});
                    return f;
                }
            }
        }
        return {};
    }

private:
    mutable std::vector<std::pair<std::string_view, IProperty::Ptr>> properties_;
    mutable std::vector<std::pair<std::string_view, IEvent::Ptr>> events_;
    mutable std::vector<std::pair<std::string_view, IFunction::Ptr>> functions_;
};

#endif // EXT_METADATA_H
