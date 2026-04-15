// direct_address_table.hpp
// =============================================================================
// Fixed-capacity map from integer keys [0, capacity) to double values,
// with O(1) clear using a generation counter.
//
// Why this structure?
// -------------------
// The BatchPQ data structure (Lemma 3.3) and the BMSSP subroutine both track
// per-vertex state that is reset at the start of each recursive call. A plain
// std::unordered_map would make clear O(size), which hurts the stated bounds.
//
// The generation-counter trick avoids that cost. Instead of writing a sentinel
// into every slot, we keep a global "current generation" integer. A slot is
// occupied iff its stored generation equals the current one. Calling clear()
// just increments the generation in O(1), which expires all slots logically.
//
// This uses global direct-address tables of size O(n) at each recursion level.
// With O(log^{1/3} n) recursion levels, total space is
// Θ(n log^{1/3} n).
//
// =============================================================================

#ifndef DIRECT_ADDRESS_TABLE_HPP
#define DIRECT_ADDRESS_TABLE_HPP

#include <vector>
#include <utility>

namespace duan25 {

class DirectAddressTable {
public:
    // Constructs a table for keys 0 .. capacity-1.
    explicit DirectAddressTable(int capacity)
        : values_(capacity, 0.0)
        , generations_(capacity, 0)
        , current_generation_(1)
    {}

    // Returns a reference to the value at key, marking the slot as present.
    // If the slot was absent the stored value is undefined. Callers must
    // assign before reading.
    double& operator[](int key) {
        generations_[key] = current_generation_;
        return values_[key];
    }

    // Returns a pointer to the value at key, or nullptr if the key is absent.
    double* find(int key) {
        if (generations_[key] != current_generation_) return nullptr;
        return &values_[key];
    }

    // Returns true if key is currently present.
    bool contains(int key) const {
        return generations_[key] == current_generation_;
    }

    // Marks key as absent.  O(1).
    void erase(int key) {
        generations_[key] = current_generation_ - 1;
    }

    // Logically removes every entry in O(1) by advancing the generation.
    void clear() {
        current_generation_++;
    }

private:
    std::vector<double> values_;
    std::vector<int>    generations_;
    int                 current_generation_;
};

} // namespace duan25

#endif // DIRECT_ADDRESS_TABLE_HPP
