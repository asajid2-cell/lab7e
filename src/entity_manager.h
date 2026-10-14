#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <array>
#include <cassert>  // assert
#include <cstddef>  // size_t
#include <limits>   // std::numeric_limits
#include <tuple>
#include <type_traits>  // std::remove_reference_t
#include <utility>      // std::forward
#include <vector>

#include "pack.h"      // Pack from prep
#include "registry.h"  // Registry

using EntityID = size_t;

// Ties the whole ECS together: which entities are alive, every component of every entity (one
// Registry per component type, each a contiguous structure of arrays), and the mapping from an
// entity to the index of each of its components.
//
// removeEntity and removeComponent are out of scope for this lab, but we still write the
// bookkeeping as if they exist: a free list of recycled entity IDs, an alive flag per entity,
// and a per-entity row of component indices that starts all-invalid and gets reset when an ID
// is recycled.
template <typename... ComponentTypes>
class EntityManager {
public:
    // An index that means "this entity has no component of this type". The spec's invalid
    // index sentinel, size_t::max() instead of the prep's -1.
    static constexpr size_t INVALID_COMPONENT_INDEX = std::numeric_limits<size_t>::max();

    // How many component types we manage.
    static constexpr size_t COMPONENT_TYPE_COUNT = sizeof...(ComponentTypes);

    // Hand out the next entity ID, recycling a freed one first since it's just sitting there.
    // A brand new entity owns nothing, so it gets a whole row of invalid indices.
    EntityID createEntity() {
        if (!mFreeEntities.empty()) {
            const EntityID id = mFreeEntities.back();
            mFreeEntities.pop_back();
            mAliveEntities[id] = true;
            mComponentIndices[id] = emptyRow();
            return id;
        }
        mComponentIndices.push_back(emptyRow());
        mAliveEntities.push_back(true);
        return mComponentIndices.size() - 1;
    }

    // True iff the entity is currently active. Takes any ID, valid or not, since there's no
    // precondition on this one.
    bool validEntity(EntityID id) const { return id < mAliveEntities.size() && mAliveEntities[id]; }

    // Does the entity have a component of this type? We look up its stored index and ask
    // whether that index is valid.
    template <typename ComponentType>
    bool hasComponent(EntityID id) const {
        assert(validEntity(id));
        return mComponentIndices[id][componentTypeIndex<ComponentType>()] != INVALID_COMPONENT_INDEX;
    }

    // The entity's component of this type, by reference so writing through it sticks. Asking
    // for one the entity doesn't have is a bug, so it trips an assertion.
    template <typename ComponentType>
    ComponentType& getComponent(EntityID id) {
        assert(validEntity(id));
        const size_t index = mComponentIndices[id][componentTypeIndex<ComponentType>()];
        assert(index != INVALID_COMPONENT_INDEX);
        return registry<ComponentType>()[index];
    }

    // Attach a component to the entity. ComponentType is a forwarding reference here, so it
    // might be a reference type; we strip that with remove_reference_t to get the real component.
    template <typename ComponentType>
    void addComponent(EntityID id, ComponentType&& component) {
        using Component = std::remove_reference_t<ComponentType>;
        assert(validEntity(id));
        assert(!hasComponent<Component>(id));
        const size_t index = registry<Component>().store(std::forward<ComponentType>(component));
        mComponentIndices[id][componentTypeIndex<Component>()] = index;
    }

private:
    // Where ComponentType sits in ComponentTypes..., checked at compile time so asking about a
    // type we don't manage fails the build instead of quietly reading the wrong column.
    template <typename ComponentType>
    static constexpr size_t componentTypeIndex() {
        static_assert(Pack<ComponentTypes...>::template contains<ComponentType>,
                      "ComponentType must be one of the managed component types.");
        return static_cast<size_t>(Pack<ComponentTypes...>::template index<ComponentType>);
    }

    // The registry holding every entity's ComponentType. The tuple is built straight from the
    // pack, so we reach the right one by its compile-time index.
    template <typename ComponentType>
    Registry<ComponentType>& registry() {
        return std::get<componentTypeIndex<ComponentType>()>(mRegistries);
    }

    // A row of invalid component indices, for a brand new (or just recycled) entity.
    static std::array<size_t, COMPONENT_TYPE_COUNT> emptyRow() {
        std::array<size_t, COMPONENT_TYPE_COUNT> row;
        row.fill(INVALID_COMPONENT_INDEX);
        return row;
    }

    std::tuple<Registry<ComponentTypes>...> mRegistries;  // one registry per component type
    std::vector<std::array<size_t, COMPONENT_TYPE_COUNT>> mComponentIndices;  // [entity][type] -> index
    std::vector<bool> mAliveEntities;                                         // per entity: still alive?
    std::vector<EntityID> mFreeEntities;                                      // recycled entity IDs
};

#endif  // ENTITY_MANAGER_H
