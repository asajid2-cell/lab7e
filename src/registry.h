#ifndef REGISTRY_H
#define REGISTRY_H

#include <cassert>  // assert
#include <cstddef>  // size_t
#include <utility>  // std::forward
#include <vector>

// Storage for one component type T, shared by every entity that has that component. This is
// the "structure of arrays" half of the ECS: all the T's live in one contiguous vector so a
// system walking every T streams past nothing else, and each stored object gets a ticket (its
// index) that doubles as its handle.
//
// Tickets handed back by remove get recycled, so we only ever grow when nothing is waiting to
// be reused, and the free list is what keeps that O(1).
template <typename T>
class Registry {
public:
    // Store a new T and hand back its ticket. We reuse a recycled ticket if one is waiting,
    // since that slot is already free to write into, and only grow by one when nothing is
    // waiting. Amortized O(1). The single forwarding parameter covers both store(const T&) and
    // store(T&&), so we don't write two overloads.
    template <typename U>
    size_t store(U&& value) {
        if (!mFreeTickets.empty()) {
            const size_t ticket = mFreeTickets.back();
            mFreeTickets.pop_back();
            mObjects[ticket] = std::forward<U>(value);
            mAlive[ticket] = true;
            ++mSize;
            return ticket;
        }
        mObjects.emplace_back(std::forward<U>(value));
        mAlive.push_back(true);
        ++mSize;
        return mObjects.size() - 1;
    }

    // Remove the object at this (live) ticket. Assigning a fresh T{} over it is what invokes
    // the old object's destruction, since the assignment releases whatever it was holding, and
    // then the ticket goes on the free list for the next store. Amortized O(1).
    void remove(size_t ticket) {
        assert(valid(ticket));
        mObjects[ticket] = T{};
        mAlive[ticket] = false;
        mFreeTickets.push_back(ticket);
        --mSize;
    }

    // The object sitting at a ticket. We assert it's live first, since a dead ticket is a bug.
    // O(1).
    T& operator[](size_t ticket) {
        assert(valid(ticket));
        return mObjects[ticket];
    }
    const T& operator[](size_t ticket) const {
        assert(valid(ticket));
        return mObjects[ticket];
    }

    // True when the ticket names a slot that both exists and is still alive. O(1), and it's
    // what the asserts above lean on.
    bool valid(size_t ticket) const { return ticket < mAlive.size() && mAlive[ticket]; }

    // How many objects are live right now. O(1), since we keep mSize as we go.
    size_t size() const { return mSize; }

    // Optional, and not marked by the lab: make room up front so the early stores don't reallocate.
    void reserve(size_t capacity) {
        mObjects.reserve(capacity);
        mAlive.reserve(capacity);
    }

private:
    std::vector<T> mObjects;           // the objects themselves, one slot each
    std::vector<bool> mAlive;          // one bit per slot: is this ticket live?
    std::vector<size_t> mFreeTickets;  // removed tickets, waiting to be reused
    size_t mSize = 0;                  // live count kept as we go, so size() stays O(1)
};

#endif  // REGISTRY_H
