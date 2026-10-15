#pragma once
// Cycle collector (the final GC). Values are still freed by reference counts
// the moment their last reference goes; this collector only finds groups of
// lists, maps, functions and scopes that point at each other but that nothing
// else can reach, and breaks them up so the counts can free them.
//
// How: every container is a GcObj on one linked list. A collection counts, for
// each object, the references that come from other containers. If its total
// count (use_count) is higher, something outside the heap (a C++ local, the
// VM stack, a global) holds it, so it is a root. Everything reachable from a
// root lives; the rest is garbage and gets cleared.
#include <cstddef>
#include <memory>
#include <vector>

namespace relic {

struct GcObj : std::enable_shared_from_this<GcObj> {
    GcObj();
    GcObj(const GcObj&);
    GcObj& operator=(const GcObj&) { return *this; }  // copies data, not list links
    virtual ~GcObj();
    // Pushes every container this object points at directly.
    virtual void trace(std::vector<GcObj*>& out) const = 0;
    // Drops every reference this object holds.
    virtual void clear_refs() = 0;

    GcObj* gc_prev = nullptr;
    GcObj* gc_next = nullptr;
};

// Runs a collection now. Gives the number of containers freed.
size_t gc_collect();
// Runs a collection if enough containers were made since the last one.
// Called at safe points: function calls and loop turns.
void gc_maybe();
size_t gc_live();  // containers alive now

}  // namespace relic
