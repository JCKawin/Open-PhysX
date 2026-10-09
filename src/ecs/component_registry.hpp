#pragma once

#include "ecs/load_report.hpp"
#include "ecs/scene.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace openphysx {

// One entry per component type. Scene copy, save, and the inspector walk this list.
// drawInspector is assigned from the UI layer. This header does not include ImGui.
struct ComponentInfo
{
    std::string name;
    int version = 1;
    bool serializable = true;
    bool replicate = true;
    bool compares = true;
    std::function<bool(const Entity&)> has;
    std::function<nlohmann::json(const Entity&)> serialize;
    std::function<void(Entity, const nlohmann::json&, LoadReport&)> deserialize;
    std::function<void(const Entity& src, Entity dst)> copy;
    std::function<void(Entity)> remove;
    std::function<bool(const Entity&, const Entity&)> equals;
    std::function<void(Entity)> drawInspector;
};

struct ComponentOptions
{
    bool serializable = true;
    bool replicate = true;
    bool compares = true;
};

class ComponentRegistry
{
public:
    static ComponentRegistry& Instance();

    template<typename T>
    void Register(std::string name, int version, ComponentOptions options = {});

    const std::vector<ComponentInfo>& All() const { return all_; }
    const ComponentInfo* Find(std::string_view name) const;
    ComponentInfo* FindMutable(std::string_view name);

private:
    std::vector<ComponentInfo> all_;
};

void EnsureComponentsRegistered();

template<typename T>
void ComponentRegistry::Register(std::string name, int version, ComponentOptions options)
{
    static_assert(std::is_default_constructible_v<T>);
    static_assert(std::is_copy_constructible_v<T>);

    for (const ComponentInfo& existing : all_)
    {
        if (existing.name == name)
            return;
    }

    ComponentInfo info;
    info.name = std::move(name);
    info.version = version;
    info.serializable = options.serializable;
    info.replicate = options.replicate;
    info.compares = options.compares;
    info.has = [](const Entity& entity) { return entity.Has<T>(); };
    info.serialize = [](const Entity& entity) {
        if (!entity.Has<T>())
            return nlohmann::json(T{});
        return nlohmann::json(entity.Get<T>());
    };
    info.deserialize = [component_name = info.name](Entity entity, const nlohmann::json& json, LoadReport& report) {
        try
        {
            T value = json.get<T>();
            if (entity.Has<T>())
                entity.Get<T>() = std::move(value);
            else
                entity.Add<T>(std::move(value));
        }
        catch (const std::exception& ex)
        {
            report.warning(component_name + " could not be read (" + ex.what() + "). Defaults were kept.");
        }
    };
    info.copy = [replicate = options.replicate](const Entity& src, Entity dst) {
        if (!replicate || !src.Has<T>())
            return;
        const T value = src.Get<T>();
        if (dst.Has<T>())
            dst.Get<T>() = value;
        else
            dst.Add<T>(value);
    };
    info.remove = [](Entity entity) { entity.Remove<T>(); };
    info.equals = [](const Entity& a, const Entity& b) {
        const bool has_a = a.Has<T>();
        const bool has_b = b.Has<T>();
        if (has_a != has_b)
            return false;
        if (!has_a)
            return true;
        return a.Get<T>() == b.Get<T>();
    };
    all_.push_back(std::move(info));
}

} // namespace openphysx
