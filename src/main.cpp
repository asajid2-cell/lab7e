#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "entity_manager.h"

// The registry on its own: tickets come out 0, 1, 2..., and a ticket we removed comes back
// out before the storage ever has to grow again.
static void testRegistry() {
    Registry<int> registry;
    const size_t a = registry.store(10);
    const size_t b = registry.store(20);
    const size_t c = registry.store(30);
    assert(a == 0 && b == 1 && c == 2);
    assert(registry.size() == 3);
    assert(registry.valid(a) && registry.valid(b) && registry.valid(c));
    assert(registry[a] == 10 && registry[b] == 20 && registry[c] == 30);

    registry.remove(b);
    assert(!registry.valid(b));
    assert(registry.size() == 2);

    const size_t d = registry.store(99);
    assert(d == b);  // the freed ticket came back first
    assert(registry[d] == 99);
    assert(registry.size() == 3);

    // remove releases what the object owned: we put a long string in, drop it, and check the
    // recycled ticket starts clean rather than still carrying the old buffer.
    Registry<std::string> names;
    const size_t n = names.store(std::string(64, 'x'));
    names.remove(n);
    const size_t n2 = names.store("short");
    assert(n2 == n);
    assert(names[n2] == "short");
}

// The example straight out of the problem statement.
static void testEntityManagerExample() {
    EntityManager<int, float, bool> entityManager;
    const EntityID e = entityManager.createEntity();
    entityManager.addComponent(e, 5);
    std::cout << entityManager.hasComponent<int>(e) << '\n';    // 1
    std::cout << entityManager.hasComponent<float>(e) << '\n';  // 0
    std::cout << entityManager.hasComponent<bool>(e) << '\n';   // 0
    std::cout << entityManager.getComponent<int>(e) << '\n';    // 5
}

// A longer run: storage grows, then a batch of removals gets recycled ticket-for-ticket
// before a single new slot is made.
static void testStress() {
    Registry<int> registry;
    std::vector<size_t> tickets;
    for (int i = 0; i < 1000; ++i) {
        tickets.push_back(registry.store(i));
    }
    assert(registry.size() == 1000);

    for (int i = 0; i < 1000; i += 2) {
        registry.remove(tickets[static_cast<size_t>(i)]);
    }
    assert(registry.size() == 500);

    for (int i = 0; i < 500; ++i) {
        const size_t ticket = registry.store(1000 + i);
        assert(ticket < 1000);  // landed on a recycled slot, no growth
        assert(registry[ticket] == 1000 + i);
    }
    assert(registry.size() == 1000);
}

// Entities, the entity -> component index mapping, and the invalid-index sentinel.
static void testEntities() {
    EntityManager<int, float, bool> entityManager;

    const EntityID e0 = entityManager.createEntity();
    const EntityID e1 = entityManager.createEntity();
    assert(e0 == 0 && e1 == 1);
    assert(entityManager.validEntity(e0) && entityManager.validEntity(e1));
    assert(!entityManager.validEntity(12345));  // takes any ID, valid or not

    // A brand new entity owns nothing at all.
    assert(!entityManager.hasComponent<int>(e0));
    assert(!entityManager.hasComponent<float>(e0));
    assert(!entityManager.hasComponent<bool>(e0));

    entityManager.addComponent(e0, 7);
    entityManager.addComponent<float>(e1, 2.5f);
    entityManager.addComponent<bool>(e0, true);

    assert(entityManager.getComponent<int>(e0) == 7);
    assert(entityManager.getComponent<float>(e1) == 2.5f);
    // We can't read a bool component back: std::vector<bool> hands out a proxy and not a real
    // bool&, so Registry<bool>::operator[] can't return the T& it promises. The spec's example
    // keeps its own getComponent<bool> line commented out too, so we leave the read to hasComponent.
    assert(entityManager.hasComponent<bool>(e0));
    assert(!entityManager.hasComponent<bool>(e1));
    assert(!entityManager.hasComponent<float>(e0));

    // Writing through the reference lands in the shared storage, not in some copy.
    entityManager.getComponent<int>(e0) = 42;
    assert(entityManager.getComponent<int>(e0) == 42);

    // An lvalue component gets copied in, not moved out of.
    int value = 11;
    entityManager.addComponent(e1, value);
    assert(value == 11);
    assert(entityManager.getComponent<int>(e1) == 11);
}

int main() {
    testRegistry();
    testEntityManagerExample();
    testEntities();
    testStress();
    std::cout << "all tests passed\n";
    return 0;
}
