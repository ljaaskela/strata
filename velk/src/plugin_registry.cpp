#include "plugin_registry.h"

#include <velk/api/velk.h>
#include <velk/ext/plugin.h>
#include <velk/interface/intf_log.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <shared_mutex>

namespace velk {

static int64_t now_us()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

PluginRegistry::PluginRegistry(IVelk& velk, TypeRegistry& types)
    : update_timestamps_{{now_us()}, {}, {}},
      log_(velk.log()),
      types_(types),
      velk_(velk)
{}

IPlugin::Ptr PluginRegistry::find_unlocked(Uid pluginId) const
{
    PluginEntry key{pluginId, {}};
    auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
    if (it != plugins_.end() && it->uid == pluginId) {
        return it->plugin;
    }
    return nullptr;
}

ReturnValue PluginRegistry::check_dependencies(const PluginInfo& info)
{
    for (auto& dep : info.dependencies) {
        auto plugin = find_unlocked(dep.uid);
        if (!plugin) {
            detail::velk_log(log_,
                             LogLevel::Error,
                             __FILE__,
                             __LINE__,
                             "Plugin '%.*s' has unmet dependency: %016llx%016llx",
                             static_cast<int>(info.name.size()),
                             info.name.data(),
                             static_cast<unsigned long long>(dep.uid.hi),
                             static_cast<unsigned long long>(dep.uid.lo));
            return ReturnValue::Fail;
        }
        if (dep.min_version && plugin->get_version() < dep.min_version) {
            detail::velk_log(log_,
                             LogLevel::Error,
                             __FILE__,
                             __LINE__,
                             "Plugin '%.*s' requires version %u.%u.%u, got %u.%u.%u",
                             static_cast<int>(info.name.size()),
                             info.name.data(),
                             version_major(dep.min_version),
                             version_minor(dep.min_version),
                             version_patch(dep.min_version),
                             version_major(plugin->get_version()),
                             version_minor(plugin->get_version()),
                             version_patch(plugin->get_version()));
            return ReturnValue::Fail;
        }
    }
    return ReturnValue::Success;
}

ReturnValue PluginRegistry::load_plugin(const IPlugin::Ptr& plugin)
{
    auto* obj = interface_cast<IObject>(plugin);
    if (!obj) {
        return ReturnValue::InvalidArgument;
    }
    Uid id = obj->get_class_uid();

    // Phase 1: check duplicate and dependencies under lock.
    {
        std::unique_lock lock(mutex_);
        PluginEntry key{id, {}};
        auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
        if (it != plugins_.end() && it->uid == id) {
            return ReturnValue::NothingToDo;
        }
        if (auto rv = check_dependencies(plugin->get_plugin_info()); failed(rv)) {
            return rv;
        }
    }

    // Phase 2: initialize plugin outside lock (callback may call back into registries).
    PluginConfig config;
    types_.set_owner(id);
    ReturnValue rv = plugin->initialize(velk_, config);
    types_.set_owner(Uid{});

    if (failed(rv)) {
        return rv;
    }

    // Phase 3: insert entry under lock (re-check for duplicate from concurrent load).
    {
        std::unique_lock lock(mutex_);
        PluginEntry key{id, {}};
        auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
        if (it != plugins_.end() && it->uid == id) {
            return ReturnValue::NothingToDo;
        }
        it = plugins_.insert(it, PluginEntry{id, plugin});
        it->config = config;

        if (config.enableUpdate) {
            update_plugins_.push_back(plugin);
        }
    }
    return ReturnValue::Success;
}

ReturnValue PluginRegistry::load_plugin(Uid pluginUid)
{
    auto plugin = interface_pointer_cast<IPlugin>(types_.create(pluginUid));
    if (!plugin) {
        detail::velk_log(log_,
                         LogLevel::Error,
                         __FILE__,
                         __LINE__,
                         "Failed to create plugin from UID %016llx%016llx",
                         static_cast<unsigned long long>(pluginUid.hi),
                         static_cast<unsigned long long>(pluginUid.lo));
        return ReturnValue::Fail;
    }
    return load_plugin(plugin);
}

ReturnValue PluginRegistry::load_plugin_from_path(const char* path)
{
    if (!path || !*path) {
        return ReturnValue::InvalidArgument;
    }

    auto lib = LibraryHandle::open(path);
    if (!lib) {
        detail::velk_log(log_, LogLevel::Error, __FILE__, __LINE__, "Failed to load library: %s", path);
        return ReturnValue::Fail;
    }

    auto* get_info = reinterpret_cast<detail::PluginInfoFn*>(lib.symbol("velk_plugin_info"));
    if (!get_info) {
        detail::velk_log(log_,
                         LogLevel::Error,
                         __FILE__,
                         __LINE__,
                         "Library missing velk_plugin_info entry point: %s",
                         path);
        lib.close();
        return ReturnValue::Fail;
    }

    auto& info = *get_info();
    Uid id = info.uid();

    // Check for duplicates and dependencies under lock before instantiating.
    {
        std::shared_lock lock(mutex_);
        PluginEntry key{id, {}};
        auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
        if (it != plugins_.end() && it->uid == id) {
            lib.close();
            return ReturnValue::NothingToDo;
        }
        if (auto rv = check_dependencies(info); failed(rv)) {
            lib.close();
            return rv;
        }
    }

    // Create the plugin instance via the factory (properly sets up control block).
    auto plugin = info.factory.create_instance<IPlugin>();
    if (!plugin) {
        detail::velk_log(
            log_, LogLevel::Error, __FILE__, __LINE__, "Factory failed to create plugin: %s", path);
        lib.close();
        return ReturnValue::Fail;
    }

    // Delegate to load_plugin which handles its own locking.
    ReturnValue rv = load_plugin(plugin);
    if (succeeded(rv)) {
        std::unique_lock lock(mutex_);
        PluginEntry key{id, {}};
        auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
        it->library = std::move(lib);
    } else {
        lib.close();
    }
    return rv;
}

ReturnValue PluginRegistry::unload_plugin(Uid pluginId)
{
    IPlugin::Ptr plugin;
    PluginConfig config;
    LibraryHandle handle;

    // Phase 1: find, check dependents, erase under lock.
    {
        std::unique_lock lock(mutex_);
        PluginEntry key{pluginId, {}};
        auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
        if (it == plugins_.end() || it->uid != pluginId) {
            return ReturnValue::InvalidArgument;
        }

        // Reject if any other loaded plugin depends on this one.
        for (auto& pe : plugins_) {
            if (pe.uid == pluginId) {
                continue;
            }
            for (auto& dep : pe.plugin->get_dependencies()) {
                if (dep.uid == pluginId) {
                    detail::velk_log(log_,
                                     LogLevel::Error,
                                     __FILE__,
                                     __LINE__,
                                     "Cannot unload plugin '%.*s': plugin '%.*s' depends on it",
                                     static_cast<int>(it->plugin->get_name().size()),
                                     it->plugin->get_name().data(),
                                     static_cast<int>(pe.plugin->get_name().size()),
                                     pe.plugin->get_name().data());
                    return ReturnValue::Fail;
                }
            }
        }

        plugin = it->plugin;
        config = it->config;
        handle = std::move(it->library);

        // Remove from update cache.
        auto uit = std::find(update_plugins_.begin(), update_plugins_.end(), plugin);
        if (uit != update_plugins_.end()) {
            update_plugins_.erase(uit);
        }

        plugins_.erase(it);
    }

    // Phase 2: shutdown and cleanup outside lock.
    plugin->shutdown(velk_);

#ifdef _DEBUG
    if (!config.retainTypesOnUnload) {
        size_t leaks = types_.check_owner_hives(pluginId);
        if (leaks > 0) {
            VELK_LOG(E, "unload_plugin: shutdown() did not release all owned objects (%zu hives with refs)",
                 leaks);
        }
    }
#endif

    if (!config.retainTypesOnUnload) {
        types_.sweep_owner(pluginId);
    }

    // Release plugin before closing the library so the destructor runs
    // while the DLL is still loaded.
    plugin.reset();
    handle.close();
    return ReturnValue::Success;
}

IPlugin::Ptr PluginRegistry::find_plugin(Uid pluginId) const
{
    std::shared_lock lock(mutex_);
    return find_unlocked(pluginId);
}

size_t PluginRegistry::plugin_count() const
{
    std::shared_lock lock(mutex_);
    return plugins_.size();
}

void PluginRegistry::shutdown_all()
{
    {
        std::unique_lock lock(mutex_);
        update_plugins_.clear();
    }

    // Unload plugins in reverse order so that dependents shut down before
    // their dependencies.
    for (;;) {
        IPlugin::Ptr plugin;
        PluginConfig config;
        LibraryHandle handle;
        Uid uid;

        {
            std::unique_lock lock(mutex_);
            if (plugins_.empty()) {
                break;
            }
            auto& entry = plugins_.back();
            plugin = entry.plugin;
            config = entry.config;
            handle = std::move(entry.library);
            uid = entry.uid;
            plugins_.pop_back();
        }

        plugin->shutdown(velk_);

#ifdef _DEBUG
        if (!config.retainTypesOnUnload) {
            size_t leaks = types_.check_owner_hives(uid);
            if (leaks > 0) {
                VELK_LOG(E, "shutdown_all: plugin shutdown() did not release all owned objects (%zu hives with refs)",
                         leaks);
            }
        }
#endif

        if (!config.retainTypesOnUnload) {
            types_.sweep_owner(uid);
        }

        // Release plugin before closing the library so the destructor runs
        // while the DLL is still loaded.
        plugin.reset();
        handle.close();
    }
}

UpdateInfo PluginRegistry::pre_update_plugins(Duration time) const
{
    bool is_explicit = time.us != 0;
    int64_t current_us = is_explicit ? time.us : now_us();
    auto& t = update_timestamps_;

    // Reset tracking when switching between explicit and auto time domains.
    if (is_explicit != last_update_was_explicit_) {
        t.elapsed = {};
        t.dt = {};
    }
    last_update_was_explicit_ = is_explicit;

    if (!t.elapsed.us) {
        t.elapsed.us = current_us;
    }

    UpdateInfo info;
    info.time = {current_us - t.time.us};
    info.elapsed = {current_us - t.elapsed.us};
    info.dt = {t.dt.us ? current_us - t.dt.us : 0};
    t.dt.us = current_us;

    // Snapshot under lock: plugins may load/unload other plugins during callbacks.
    vector<IPlugin::Ptr> plugins;
    {
        std::shared_lock lock(mutex_);
        plugins = update_plugins_;
    }
    for (auto& plugin : plugins) {
        plugin->pre_update({info});
    }

    return info;
}

void PluginRegistry::post_update_plugins(const IPlugin::PostUpdateInfo& info) const
{
    vector<IPlugin::Ptr> plugins;
    {
        std::shared_lock lock(mutex_);
        plugins = update_plugins_;
    }
    for (auto& plugin : plugins) {
        plugin->post_update(info);
    }
}

} // namespace velk
