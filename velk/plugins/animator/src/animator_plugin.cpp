#include "animator_plugin.h"

#include <velk/ext/any.h>

namespace velk {

ReturnValue AnimatorPlugin::initialize(IVelk& velk, PluginConfig& config)
{
    config.enableUpdate = true;
    auto rv = register_type<impl::AnimationTrack>(velk);
    rv &= register_type<impl::Animator>(velk);
    rv &= register_type<impl::Transition>(velk);
    rv &= ::velk::register_type<AnimatorImportHandler>(velk);
    auto& types = velk.type_registry();
    rv &= types.register_type<ext::AnyValue<KeyframeEntry>>();
    rv &= types.register_interpolator<float>(&detail::typed_interpolator<float>);
    rv &= types.register_interpolator<double>(&detail::typed_interpolator<double>);
    rv &= types.register_interpolator<uint8_t>(&detail::typed_interpolator<uint8_t>);
    rv &= types.register_interpolator<uint16_t>(&detail::typed_interpolator<uint16_t>);
    rv &= types.register_interpolator<uint32_t>(&detail::typed_interpolator<uint32_t>);
    rv &= types.register_interpolator<uint64_t>(&detail::typed_interpolator<uint64_t>);
    rv &= types.register_interpolator<int8_t>(&detail::typed_interpolator<int8_t>);
    rv &= types.register_interpolator<int16_t>(&detail::typed_interpolator<int16_t>);
    rv &= types.register_interpolator<int32_t>(&detail::typed_interpolator<int32_t>);
    rv &= types.register_interpolator<int64_t>(&detail::typed_interpolator<int64_t>);

    animator_ = velk.create<IAnimator>(ClassId::Animator);
    velk_ = &velk;
    return animator_ ? rv : ReturnValue::Fail;
}

ReturnValue AnimatorPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

void AnimatorPlugin::pre_update(const IPlugin::PreUpdateInfo& info)
{
    if (auto* a = interface_cast<IAnimator>(animator_)) {
        a->tick(info.info);
    }
}

} // namespace velk
