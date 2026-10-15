#include "interp/gc.hpp"

#include <unordered_map>

namespace relic {

namespace {

GcObj* g_head = nullptr;
size_t g_live = 0;
size_t g_threshold = 100000;
constexpr size_t kMinThreshold = 100000;

}  // namespace

GcObj::GcObj() {
    gc_next = g_head;
    if (g_head) g_head->gc_prev = this;
    g_head = this;
    ++g_live;
}

GcObj::GcObj(const GcObj&) : GcObj() {}

GcObj::~GcObj() {
    if (gc_prev) gc_prev->gc_next = gc_next;
    else g_head = gc_next;
    if (gc_next) gc_next->gc_prev = gc_prev;
    --g_live;
}

size_t gc_live() { return g_live; }

size_t gc_collect() {
    // Only objects owned by a shared_ptr take part; a container on the C++
    // stack is not in the heap, so what it points at counts as held from outside.
    std::vector<GcObj*> objs;
    std::unordered_map<GcObj*, size_t> index;
    for (GcObj* o = g_head; o; o = o->gc_next) {
        if (o->weak_from_this().expired()) continue;
        index[o] = objs.size();
        objs.push_back(o);
    }
    std::vector<long> inner(objs.size(), 0);
    std::vector<GcObj*> kids;
    for (GcObj* o : objs) {
        kids.clear();
        o->trace(kids);
        for (GcObj* k : kids) {
            auto it = index.find(k);
            if (it != index.end()) ++inner[it->second];
        }
    }
    std::vector<char> live(objs.size(), 0);
    std::vector<size_t> work;
    for (size_t i = 0; i < objs.size(); ++i) {
        if (objs[i]->weak_from_this().use_count() > inner[i]) {
            live[i] = 1;
            work.push_back(i);
        }
    }
    while (!work.empty()) {
        size_t i = work.back();
        work.pop_back();
        kids.clear();
        objs[i]->trace(kids);
        for (GcObj* k : kids) {
            auto it = index.find(k);
            if (it != index.end() && !live[it->second]) {
                live[it->second] = 1;
                work.push_back(it->second);
            }
        }
    }
    std::vector<std::shared_ptr<GcObj>> dead;
    for (size_t i = 0; i < objs.size(); ++i)
        if (!live[i]) dead.push_back(objs[i]->shared_from_this());
    for (auto& d : dead) d->clear_refs();
    size_t freed = dead.size();
    dead.clear();  // the last references go here
    g_threshold = g_live * 2 > kMinThreshold ? g_live * 2 : kMinThreshold;
    return freed;
}

void gc_maybe() {
    if (g_live > g_threshold) gc_collect();
}

}  // namespace relic
