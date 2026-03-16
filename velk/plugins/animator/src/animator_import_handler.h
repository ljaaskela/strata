#ifndef VELK_ANIMATOR_IMPORT_HANDLER_H
#define VELK_ANIMATOR_IMPORT_HANDLER_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>

namespace velk {

/**
 * @brief Importer extension that handles the "animations" top-level collection.
 *
 * Each entry creates a transition on a target property resolved from the store.
 *
 * JSON format:
 *   "animations": [
 *       {
 *           "type": "transition",
 *           "targets": ["object_id.property_name", ...],
 *           "duration": 0.5,
 *           "easing": "out_cubic"
 *       },
 *       {
 *           "type": "track",
 *           "targets": ["object_id.property_name", ...],
 *           "keyframes": [
 *               { "time": 0.0, "value": 0.0 },
 *               { "time": 0.5, "value": 50.0, "easing": "out_cubic" },
 *               { "time": 1.0, "value": 100.0 }
 *           ],
 *           "autoplay": true
 *       }
 *   ]
 *
 * Supported easing names: linear, in_quad, out_quad, in_out_quad,
 * in_cubic, out_cubic, in_out_cubic, in_sine, out_sine, in_out_sine,
 * in_expo, out_expo, in_out_expo, in_elastic, out_elastic,
 * in_bounce, out_bounce.
 */
class AnimatorImportHandler : public ext::ObjectCore<AnimatorImportHandler, IImporterExtension>
{
public:
    VELK_CLASS_UID("a1b2c3d4-e5f6-7890-abcd-ef0123456789", "AnimatorImportHandler");

    string_view collection_key() const override;
    void process(const IImportData& data, IStore& store,
                 const IImportResolver& resolver) const override;
};

} // namespace velk

#endif // VELK_ANIMATOR_IMPORT_HANDLER_H
