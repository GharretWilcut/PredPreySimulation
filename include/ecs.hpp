#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <cassert>

// basic Entity type
using Entity = uint32_t;
static constexpr Entity INVALID_ENTITY = 0;

class World {
public:
    World() : next_id(1) {}
    Entity create_entity() { return next_id++; }

    // Component storage: map of (type_index -> map<Entity, void*>)
    template<typename Comp>
    std::unordered_map<Entity, Comp>& get_comp_store() {
        auto idx = std::type_index(typeid(Comp));
        if (stores.find(idx) == stores.end()) {
            stores[idx] = std::make_shared<AnyStore<Comp>>();
        }
        auto base = std::static_pointer_cast<AnyStore<Comp>>(stores[idx]);
        return base->map;
    }

    template<typename Comp>
    bool has(Entity e) {
        auto &m = get_comp_store<Comp>();
        return m.find(e) != m.end();
    }

    template<typename Comp, typename... Args>
    Comp& add(Entity e, Args&&... args) {
        auto &m = get_comp_store<Comp>();
        auto res = m.emplace(e, Comp{ std::forward<Args>(args)... });
        return res.first->second;
    }

    template<typename Comp>
    void remove(Entity e) {
        auto &m = get_comp_store<Comp>();
        m.erase(e);
    }

    template<typename Comp>
    Comp* get(Entity e) {
        auto &m = get_comp_store<Comp>();
        auto it = m.find(e);
        if (it == m.end()) return nullptr;
        return &it->second;
    }

    template<typename Comp>
    std::vector<std::pair<Entity, Comp*>> all() {
        std::vector<std::pair<Entity, Comp*>> out;
        auto &m = get_comp_store<Comp>();
        out.reserve(m.size());
        for (auto &kv : m) out.emplace_back(kv.first, &kv.second);
        return out;
    }

private:
    Entity next_id;
    struct IAnyStore { virtual ~IAnyStore() = default; };
    template<typename Comp>
    struct AnyStore : IAnyStore { std::unordered_map<Entity, Comp> map; };
    std::unordered_map<std::type_index, std::shared_ptr<IAnyStore>> stores;
};
