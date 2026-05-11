#include "tracy_perf_sink.h"

#include <velk/vector.h>

#include <tracy/TracyC.h>

#include <cstring>

namespace velk::impl {

struct ZoneEntry
{
    uint64_t key;
    TracyCZoneCtx ctx;
};

static vector<ZoneEntry>& thread_zones()
{
    thread_local vector<ZoneEntry> zones;
    return zones;
}

static constexpr bool starts_with(string_view str, string_view prefix)
{
    if (str.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (str.data()[i] != prefix.data()[i]) return false;
    }
    return true;
}

static constexpr uint32_t color_for_label(string_view label)
{
    // Tracy colors are 0xRRGGBB. Tracy renders zones on a pale
    // background, so saturated mid-darks (~30 to 50% lightness)
    // read clearly. Distinct hues per category.
    if (starts_with(label, "renderer.")) return 0xC03020;  // dark orange-red
    if (starts_with(label, "vk."))       return 0x800020;  // burgundy
    if (starts_with(label, "scene."))    return 0x1F4E79;  // dark blue
    if (starts_with(label, "layout."))   return 0x2E7D32;  // forest green
    if (starts_with(label, "text."))     return 0x6A1B9A;  // dark purple
    if (starts_with(label, "image."))    return 0xB7791F;  // dark amber
    if (starts_with(label, "app."))      return 0x00838F;  // dark teal
    return 0;
}

void TracyPerfSink::start_perf(uint64_t key, string_view label,
                               const char* file, uint32_t line)
{
    uint64_t srcloc = ___tracy_alloc_srcloc_name(
        line,
        file ? file : "", file ? strlen(file) : 0,
        "", 0,
        label.data(), label.size(),
        0);
    TracyCZoneCtx ctx = ___tracy_emit_zone_begin_alloc(srcloc, 1);

    uint32_t color = color_for_label(label);
    if (color) {
        ___tracy_emit_zone_color(ctx, color);
    }

    thread_zones().push_back({key, ctx});
}

void TracyPerfSink::end_perf(uint64_t key, string_view, Duration)
{
    auto& zones = thread_zones();
    for (auto it = zones.end(); it != zones.begin();) {
        --it;
        if (it->key == key) {
            ___tracy_emit_zone_end(it->ctx);
            zones.erase(it);
            return;
        }
    }
}

void TracyPerfSink::event(PerfEvent type)
{
    switch (type) {
    case PerfEvent::Update:  ___tracy_emit_frame_mark("Update"); break;
    case PerfEvent::Render:  ___tracy_emit_frame_mark("Render"); break;
    case PerfEvent::Present: ___tracy_emit_frame_mark(nullptr); break;
    }
}

} // namespace velk::impl
