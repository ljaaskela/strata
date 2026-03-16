#include "animator_import_handler.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_store.h>
#include <velk/plugins/animator/easing.h>
#include <velk/plugins/animator/interface/intf_animation_track.h>
#include <velk/plugins/animator/interface/intf_animator.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>
#include <velk/plugins/animator/interface/intf_transition.h>
#include <velk/plugins/animator/plugin.h>

#include <string>
#include <vector>

namespace velk {

namespace {

easing::EasingFn resolve_easing(string_view name)
{
    if (name == "linear") return easing::linear;
    if (name == "in_quad") return easing::in_quad;
    if (name == "out_quad") return easing::out_quad;
    if (name == "in_out_quad") return easing::in_out_quad;
    if (name == "in_cubic") return easing::in_cubic;
    if (name == "out_cubic") return easing::out_cubic;
    if (name == "in_out_cubic") return easing::in_out_cubic;
    if (name == "in_sine") return easing::in_sine;
    if (name == "out_sine") return easing::out_sine;
    if (name == "in_out_sine") return easing::in_out_sine;
    if (name == "in_expo") return easing::in_expo;
    if (name == "out_expo") return easing::out_expo;
    if (name == "in_out_expo") return easing::in_out_expo;
    if (name == "in_elastic") return easing::in_elastic;
    if (name == "out_elastic") return easing::out_elastic;
    if (name == "in_bounce") return easing::in_bounce;
    if (name == "out_bounce") return easing::out_bounce;
    return easing::linear;
}

IProperty::Ptr resolve_property(const IImportResolver& resolver, string_view path)
{
    auto obj = resolver.resolve(path);
    return obj ? interface_pointer_cast<IProperty>(obj) : IProperty::Ptr{};
}

// Creates an IAny value matching the property's type and sets it from the import data node.
IAny::Ptr create_value_from_data(const IProperty::Ptr& prop, const IImportData& node)
{
    if (!prop || node.is_null()) {
        return {};
    }

    auto val = prop->get_value();
    if (!val) {
        return {};
    }

    auto types = val->get_compatible_types();
    if (types.empty()) {
        return {};
    }

    Uid type = types[0];
    auto any = ::velk::instance().type_registry().create_any(type);
    if (!any) {
        return {};
    }

    if (type == type_uid<float>()) {
        float v = static_cast<float>(node.as_number());
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<double>()) {
        double v = node.as_number();
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<int32_t>()) {
        int32_t v = static_cast<int32_t>(node.as_number());
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<uint32_t>()) {
        uint32_t v = static_cast<uint32_t>(node.as_number());
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<int64_t>()) {
        int64_t v = static_cast<int64_t>(node.as_number());
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<uint64_t>()) {
        uint64_t v = static_cast<uint64_t>(node.as_number());
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<int>()) {
        int v = static_cast<int>(node.as_number());
        any->set_data(&v, sizeof(v), type);
    } else if (type == type_uid<bool>()) {
        bool v = node.as_bool();
        any->set_data(&v, sizeof(v), type);
    } else {
        return {};
    }

    return any;
}

void process_transition(const IImportData& entry, IStore& store, const IImportResolver& resolver,
                        size_t index)
{
    auto& targets_node = entry.find("targets");
    if (targets_node.is_null() || targets_node.count() == 0) {
        return;
    }

    double duration_sec = entry.find("duration").as_number();

    easing::EasingFn ease = easing::linear;
    auto& easing_node = entry.find("easing");
    if (!easing_node.is_null()) {
        ease = resolve_easing(easing_node.as_string());
    }

    auto obj = ::velk::instance().create<IObject>(ClassId::Transition);
    auto tr = interface_pointer_cast<ITransition>(obj);
    if (!tr) {
        return;
    }

    tr->set_easing(ease);
    tr->duration().set_value(Duration::from_seconds(static_cast<float>(duration_sec)));
    tr->set_transient(true);

    for (size_t t = 0; t < targets_node.count(); t++) {
        auto target_sv = targets_node.at(t).as_string();
        if (target_sv.empty()) {
            continue;
        }
        auto prop = resolve_property(resolver, target_sv);
        if (prop) {
            tr->add_target(prop);
        }
    }

    auto store_key = "animation:" + std::to_string(index);
    store.add(string_view(store_key.c_str(), store_key.size()),
              interface_pointer_cast<IObject>(tr));
}

void process_track(const IImportData& entry, IStore& store, const IImportResolver& resolver,
                   size_t index)
{
    auto& targets_node = entry.find("targets");
    if (targets_node.is_null() || targets_node.count() == 0) {
        return;
    }

    auto& keyframes_node = entry.find("keyframes");
    if (keyframes_node.is_null() || keyframes_node.count() == 0) {
        return;
    }

    // Resolve first target to determine the property type for keyframe values
    IProperty::Ptr first_prop;
    for (size_t t = 0; t < targets_node.count(); t++) {
        auto target_sv = targets_node.at(t).as_string();
        if (!target_sv.empty()) {
            first_prop = resolve_property(resolver, target_sv);
            if (first_prop) {
                break;
            }
        }
    }
    if (!first_prop) {
        return;
    }

    // Build keyframes
    vector<KeyframeEntry> keyframes;
    for (size_t k = 0; k < keyframes_node.count(); k++) {
        auto& kf_node = keyframes_node.at(k);

        double time_sec = kf_node.find("time").as_number();
        auto& value_node = kf_node.find("value");
        if (value_node.is_null()) {
            continue;
        }

        auto value = create_value_from_data(first_prop, value_node);
        if (!value) {
            continue;
        }

        easing::EasingFn ease = easing::linear;
        auto& ease_node = kf_node.find("easing");
        if (!ease_node.is_null()) {
            ease = resolve_easing(ease_node.as_string());
        }

        keyframes.push_back({Duration::from_seconds(static_cast<float>(time_sec)),
                             std::move(value), ease});
    }

    if (keyframes.empty()) {
        return;
    }

    // Create the animation track
    auto obj = ::velk::instance().create<IObject>(ClassId::AnimationTrack);
    auto track = interface_pointer_cast<IAnimationTrack>(obj);
    if (!track) {
        return;
    }

    track->set_keyframes({keyframes.data(), keyframes.size()});
    track->set_transient(true);

    // Add all targets
    for (size_t t = 0; t < targets_node.count(); t++) {
        auto target_sv = targets_node.at(t).as_string();
        if (target_sv.empty()) {
            continue;
        }
        auto prop = resolve_property(resolver, target_sv);
        if (prop) {
            track->add_target(prop);
        }
    }

    // Register with the default animator
    auto ap = ::velk::get_or_load_plugin<IAnimatorPlugin>(PluginId::AnimatorPlugin);
    if (ap) {
        ap->get_default_animator().add(track);
    }

    // Autoplay (default true)
    auto& autoplay_node = entry.find("autoplay");
    bool autoplay = autoplay_node.is_null() || autoplay_node.as_bool();
    if (autoplay) {
        track->play();
    }

    auto store_key = "animation:" + std::to_string(index);
    store.add(string_view(store_key.c_str(), store_key.size()),
              interface_pointer_cast<IObject>(track));
}

} // namespace

string_view AnimatorImportHandler::collection_key() const
{
    return "animations";
}

void AnimatorImportHandler::process(const IImportData& data, IStore& store,
                                    const IImportResolver& resolver) const
{
    for (size_t i = 0; i < data.count(); i++) {
        auto& entry = data.at(i);

        auto type_sv = entry.find("type").as_string();
        if (type_sv == "transition") {
            process_transition(entry, store, resolver, i);
        } else if (type_sv == "track") {
            process_track(entry, store, resolver, i);
        }
    }
}

} // namespace velk
