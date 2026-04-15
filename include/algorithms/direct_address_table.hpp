// direct_address_table.hpp
// =============================================================================
// A fixed-capacity map from integer keys [0, capacity) to double values,
// supporting O(1) clear via a generation counter.
//
// Why this structure?
// -------------------
// The BatchPQ data structure (Lemma 3.3) and the BMSSP subroutine both need
// to track per-vertex information that must be reset at the start of each
// recursive call.  A naive std::unordered_map would charge O(size) for clear,
// which would break the time bounds.
//
// The generation-counter trick avoids this: rather than writing a sentinel
// value into every slot, we keep a global "current generation" integer.  A
// slot is considered occupied if and only if its stored generation equals the
// current one.  Calling clear() simply increments the current generation in
// O(1), logically expiring every slot at once.
//
// This is the approach described in Section 4 of Castro et al. (2025):
//   "we employ global direct-address tables (DATs) of size O(n) ... with
//    O(log^{1/3} n) recursion levels, the total space complexity becomes
//    Θ(n log^{1/3} n)."
//
// Limitations
// -----------
//  - Keys must be non-negative integers strictly less than the capacity
//    supplied to the constructor.
//  - The generation counter wraps at INT_MAX.  For typical algorithm runs
//    this is not a concern.
//  - Iteration over all live entries is not supported.
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
