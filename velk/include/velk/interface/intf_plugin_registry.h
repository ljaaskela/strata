#ifndef VELK_INTF_PLUGIN_REGISTRY_H
#define VELK_INTF_PLUGIN_REGISTRY_H

#include <velk/interface/intf_plugin.h>
#include <velk/string_view.h>

namespace velk {

/**
 * @brief Interface for managing loaded plugins.
 *
 * Plugins are loaded via load_plugin() which calls their initialize() method,
 * and unloaded via unload_plugin() which calls shutdown() and sweeps any
 * types the plugin registered but did not explicitly unregister.
 */
class IPluginRegistry : public Interface<IPluginRegistry>
{
public:
    /**
     * @brief Loads a plugin by URI. Primary entry point for plugin loading.
     *
     * Currently supports the `plugin:` scheme: `plugin:NAME` resolves to
     * `<exe_dir>/plugins/<NAME>.dll` on Windows or `<exe_dir>/plugins/lib<NAME>.so`
     * on other platforms, then loads the resulting shared library. Lets callers
     * use the same URI on every platform without per-OS path handling.
     */
    virtual ReturnValue load_plugin(string_view uri) = 0;
    /** @brief Loads an already-instantiated plugin object, calling its initialize() method. */
    virtual ReturnValue load_plugin(const IPlugin::Ptr& plugin) = 0;
    /** @brief Creates and loads a plugin by its registered class UID. */
    virtual ReturnValue load_plugin_from_uid(Uid pluginUid) = 0;
    /** @brief Loads a plugin from a shared library (.dll/.so) at the given path. */
    virtual ReturnValue load_plugin_from_path(string_view path) = 0;
    /** @brief Unloads a plugin by ID, calling shutdown() and sweeping owned types. */
    virtual ReturnValue unload_plugin(Uid pluginId) = 0;
    /** @brief Finds a loaded plugin by its ID, or nullptr if not loaded. */
    virtual IPlugin::Ptr find_plugin(Uid pluginId) const = 0;
    /** @brief Returns the number of currently loaded plugins. */
    virtual size_t plugin_count() const = 0;

    /** @brief Unloads a plugin by its class type. */
    template <class T>
    ReturnValue unload_plugin()
    {
        return unload_plugin(T::static_class_id());
    }
    /** @brief Finds a loaded plugin by its class type, or nullptr if not loaded. */
    template <class T>
    IPlugin::Ptr find_plugin() const
    {
        return find_plugin(T::static_class_id());
    }
    /**
     * @brief Returns a plugin instance with given plugin id.
     *        Lazily loads the plugin if not already loaded.
     * @param pluginId Id if the plugin.
     * @return Plugin instance or nullptr if loading failed.
     */
    IPlugin::Ptr get_or_load_plugin(Uid pluginId)
    {
        auto plugin = find_plugin(pluginId);
        if (!plugin) {
            load_plugin_from_uid(pluginId);
            plugin = find_plugin(pluginId);
        }
        return plugin;
    }
};

} // namespace velk

#endif // VELK_INTF_PLUGIN_REGISTRY_H
