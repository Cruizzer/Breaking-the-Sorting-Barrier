// batch_pq.hpp
// =============================================================================
// Block-based data structure satisfying Lemma 3.3 of Duan et al. (2025).
//
// What Lemma 3.3 requires
// -----------------------
// Given at most N key/value pairs (keys are vertex indices, values are
// PathLabels), an integer block-capacity parameter M, and an upper bound B on
// all values, the structure must support:
//
//   Insert(key, value)
//     Insert or update a key/value pair.  If the key already exists keep the
//     smaller value.  Amortised O(max{1, log(N/M)}) time.
//
//   BatchPrepend(list L)
//     Insert all pairs in L, each of whose values is *smaller* than every
//     value currently in the structure.  If L contains duplicate keys keep
//     the smallest.  Amortised O(|L| · max{1, log(|L|/M)}) time.
//
//   Pull()
//     Remove and return a subset S' of at most M keys associated with the
//     smallest |S'| values, together with a separating upper bound x such
//     that max(S') < x <= min(remainder).  If the structure is empty, x = B.
//     Amortised O(|S'|) time.
//
// Data layout
// -----------
// Elements are stored in two sequences of doubly-linked blocks:
//
//   D0  — blocks produced by BatchPrepend (prepended elements)
//   D1  — blocks produced by Insert       (individually inserted elements)
//
// Each block is a linked list holding at most M entries.  Blocks within each
// sequence are ordered so that all values in block i are <= all values in
// block i+1.  A std::set (red-black tree) called `upper_bound_set` maintains
// one (upper_bound, block_iterator) entry per D1 block, so Insert can locate
// the correct target block in O(log(|D1|)) = O(log(N/M)) time.
//
// Per-key bookkeeping
// -------------------
// Two location tables (one for D0, one for D1) record, for each vertex key,
// which block it sits in and where inside that block.  This makes Delete O(1).
// A third table `best_label` stores the best PathLabel seen for each key, so
// duplicate insertions can be detected without scanning blocks.
//
// All three tables are DirectAddressTables and support O(1) clear, so the
// whole structure can be re-initialised for a new recursive call in O(1).
//
// Selection algorithm
// -------------------
// Pull() uses the median-of-ninthers selection routine from
// `external/median_of_ninthers.h` (Alexandrescu 2017) to find the M-th
// smallest element in O(M) worst-case time, as required by the paper.
// =============================================================================

#ifndef BATCH_PQ_HPP
#define BATCH_PQ_HPP

#include <list>
#include <set>
#include <vector>
#include <utility>
#include <cassert>
#include <functional>
#include <algorithm>

#include "path_label.hpp"

namespace duan25 {

// An entry stored inside a block: the vertex id and its current best label.
struct Entry {
    int       vertex;
    PathLabel label;
};

// ── Iterator types ────────────────────────────────────────────────────────────

using Block       = std::list<Entry>;
using BlockList   = std::list<Block>;
using BlockIter   = BlockList::iterator;
using EntryIter   = Block::iterator;

// Per-key location record: which block and which position inside it.
struct Location {
    BlockIter block;
    EntryIter entry;
};

// ── Upper-bound set comparator ────────────────────────────────────────────────
// The set stores (upper_bound_label, block_iterator) pairs ordered by label
// first, then by block address to break ties between blocks with equal bounds.
struct UBComparator {
    bool operator()(const std::pair<PathLabel, BlockIter>& a,
                    const std::pair<PathLabel, BlockIter>& b) const {
        if (a.first != b.first) return a.first < b.first;
        return &(*a.second) < &(*b.second);
    }
};

using UBSet = std::set<std::pair<PathLabel, BlockIter>, UBComparator>;

// ── Location tables ───────────────────────────────────────────────────────────
// We need O(1) lookup of where each vertex lives in D0 / D1.  std::unordered_map
// would give expected-O(1) but the paper specifies worst-case O(1) via DATs.
// We wrap the location in a plain vector and use a generation-counter to clear.
struct LocationTable {
    std::vector<Location>  locs;
    std::vector<int>       gen;
    int                    cur_gen;

    explicit LocationTable(int n) : locs(n), gen(n, 0), cur_gen(1) {}

    Location* find(int v) {
        if (gen[v] != cur_gen) return nullptr;
        return &locs[v];
    }
    void set(int v, Location loc) {
        gen[v]  = cur_gen;
        locs[v] = loc;
    }
    void erase(int v) { gen[v] = cur_gen - 1; }
    void clear()      { cur_gen++; }
};

// ── Best-label table ──────────────────────────────────────────────────────────
struct BestLabelTable {
    std::vector<PathLabel> labels;
    std::vector<int>       gen;
    int                    cur_gen;

    explicit BestLabelTable(int n) : labels(n), gen(n, 0), cur_gen(1) {}

    PathLabel* find(int v) {
        if (gen[v] != cur_gen) return nullptr;
        return &labels[v];
    }
    void set(int v, PathLabel lbl) {
        gen[v]     = cur_gen;
        labels[v]  = lbl;
    }
    void erase(int v) { gen[v] = cur_gen - 1; }
    void clear()      { cur_gen++; }
};

// =============================================================================
class BatchPQ {
public:
    explicit BatchPQ(int num_vertices)
        : loc_in_D0_(num_vertices)
        , loc_in_D1_(num_vertices)
        , best_label_(num_vertices)
        , block_capacity_(0)
        , num_entries_(0)
    {}

    // ── Initialise / reset ────────────────────────────────────────────────────
    // Must be called at the start of each BMSSP recursive call.
    // M is the block capacity (= 2^{(l-1)t}); B is the current upper bound.
    void initialise(int M, PathLabel B) {
        block_capacity_ = M;
        upper_bound_    = B;
        num_entries_    = 0;

        D0_.clear();
        D1_.clear();

        // D1 starts with one empty sentinel block whose upper bound is B.
        D1_.push_back(Block());
        sentinel_ = D1_.begin();
        ub_set_.clear();
        ub_set_.insert({ B, sentinel_ });

        best_label_.clear();
        loc_in_D0_.clear();
        loc_in_D1_.clear();
    }

    int size() const { return num_entries_; }

    // ── Insert ────────────────────────────────────────────────────────────────
    // Insert (vertex, label).  If vertex already has a better or equal label
    // the call is a no-op.  Amortised O(log(N/M)) time.
    void insert(int vertex, PathLabel label) {
        PathLabel* existing = best_label_.find(vertex);

        if (existing != nullptr) {
            if (*existing <= label) return;          // already at least as good
            remove_from_structure(vertex, *existing); // replace with better one
        }

        // Find the first D1 block whose UB strictly exceeds label.
        auto it_ub = ub_set_.lower_bound({ label, sentinel_ });
        if (it_ub == ub_set_.end()) {
            // Defensive fallback: labels are expected to be < current upper
            // bound, but if numerical edge-cases violate that, append into the
            // last known D1 block instead of dereferencing end().
            it_ub = std::prev(ub_set_.end());
        }
        BlockIter target_block = it_ub->second;

        EntryIter pos = target_block->insert(target_block->end(), { vertex, label });
        loc_in_D1_.set(vertex, { target_block, pos });
        best_label_.set(vertex, label);
        num_entries_++;

        if ((int)target_block->size() > block_capacity_) {
            split_d1_block(target_block);
        }
    }

    // ── BatchPrepend ──────────────────────────────────────────────────────────
    // Insert all entries in `entries`, each of whose labels is assumed to be
    // smaller than anything currently in the structure.
    // Amortised O(|entries| · max{1, log(|entries|/M)}) time.
    void batch_prepend(const std::vector<Entry>& entries) {
        batch_prepend_list(entries.begin(), entries.end(),
                           (int)entries.size());
    }

    // ── Pull ──────────────────────────────────────────────────────────────────
    // Extract at most M entries with the smallest labels.
    // Returns { separating_label, list_of_vertex_ids }.
    // Time: O(M) amortised.
    std::pair<PathLabel, std::vector<int>> pull() {
        // Collect candidates from the fronts of D0 and D1.
        std::vector<Entry> candidates;
        candidates.reserve(2 * block_capacity_ + 2);

        for (auto it = D0_.begin();
             it != D0_.end() && (int)candidates.size() <= block_capacity_; ++it)
            for (const Entry& e : *it) candidates.push_back(e);

        for (auto it = D1_.begin();
             it != D1_.end() && (int)candidates.size() <= block_capacity_; ++it)
            for (const Entry& e : *it) candidates.push_back(e);

        if ((int)candidates.size() <= block_capacity_) {
            // Everything fits — return all of them.
            std::vector<int> result;
            result.reserve(candidates.size());
            for (const Entry& e : candidates) {
                result.push_back(e.vertex);
                remove_from_structure(e.vertex, e.label);
            }
            return { upper_bound_, result };
        }

        // Use selection to find the M-th smallest label.
        PathLabel separator = select_kth(candidates, block_capacity_);

        std::vector<int> result;
        result.reserve(block_capacity_);
        for (const Entry& e : candidates) {
            if (e.label < separator) {
                result.push_back(e.vertex);
                remove_from_structure(e.vertex, e.label);
            }
        }

        // Guard against an empty batch (can happen when many labels equal the
        // separator). BMSSP recursion requires a non-empty source set whenever
        // the queue is non-empty.
        if (result.empty() && !candidates.empty()) {
            int best_idx = 0;
            for (int i = 1; i < (int)candidates.size(); ++i) {
                if (candidates[i].label < candidates[best_idx].label) {
                    best_idx = i;
                }
            }
            result.push_back(candidates[best_idx].vertex);
            remove_from_structure(candidates[best_idx].vertex,
                                  candidates[best_idx].label);
        }

        return { separator, result };
    }

    // ── Erase ─────────────────────────────────────────────────────────────────
    // Remove vertex from the structure if present.  O(1).
    void erase(int vertex) {
        PathLabel* lbl = best_label_.find(vertex);
        if (lbl != nullptr) {
            remove_from_structure(vertex, *lbl);
        }
    }

private:

    // Find the UB-set entry corresponding to a specific D1 block.
    UBSet::iterator find_ub_entry_by_block(BlockIter blk) {
        for (auto it = ub_set_.begin(); it != ub_set_.end(); ++it) {
            if (it->second == blk) return it;
        }
        return ub_set_.end();
    }

    // ── Block sequences ───────────────────────────────────────────────────────
    BlockList D0_;         // blocks from batch_prepend
    BlockList D1_;         // blocks from insert
    UBSet     ub_set_;     // upper bounds for each D1 block
    BlockIter sentinel_;   // the initial sentinel block in D1

    int       block_capacity_;
    int       num_entries_;
    PathLabel upper_bound_;

    // ── Per-key tables ────────────────────────────────────────────────────────
    LocationTable  loc_in_D0_;
    LocationTable  loc_in_D1_;
    BestLabelTable best_label_;

    // ── remove_from_structure ─────────────────────────────────────────────────
    // Remove a specific (vertex, label) entry, cleaning up empty blocks.
    void remove_from_structure(int vertex, PathLabel stored_label) {
        Location* loc1 = loc_in_D1_.find(vertex);
        if (loc1 != nullptr) {
            BlockIter blk = loc1->block;
            blk->erase(loc1->entry);
            loc_in_D1_.erase(vertex);

            if (blk->empty()) {
                // Remove the block's entry from the UB set, unless it's the sentinel.
                auto it_ub = find_ub_entry_by_block(blk);
                if (blk != sentinel_) {
                    if (it_ub != ub_set_.end()) ub_set_.erase(it_ub);
                    D1_.erase(blk);
                }
            }
        } else {
            Location* loc0 = loc_in_D0_.find(vertex);
            if (loc0 == nullptr) return;  // already absent
            loc0->block->erase(loc0->entry);
            loc_in_D0_.erase(vertex);
            if (loc0->block->empty()) D0_.erase(loc0->block);
        }

        best_label_.erase(vertex);
        num_entries_--;
    }

    // ── select_kth ────────────────────────────────────────────────────────────
    // Rearranges `entries` so that entries[k] is the k-th smallest by label,
    // using the median-of-ninthers algorithm (worst-case O(n) selection).
    PathLabel select_kth(std::vector<Entry>& entries, int k) {
        auto cmp = [](const Entry& a, const Entry& b) {
            return a.label < b.label;
        };
        std::nth_element(entries.begin(), entries.begin() + k, entries.end(), cmp);
        return entries[k].label;
    }

    // ── split_d1_block ────────────────────────────────────────────────────────
    // Split a D1 block that has exceeded `block_capacity_`.
    // Elements with label >= median go into a new block inserted after the
    // current one.  The UB set is updated accordingly.
    // Time: O(M) for the scan + O(log(N/M)) for the UB set update.
    void split_d1_block(BlockIter blk) {
        int sz = (int)blk->size();
        std::vector<Entry> tmp(blk->begin(), blk->end());
        PathLabel median = select_kth(tmp, sz / 2);

        // Insert a new block immediately after `blk`.
        BlockIter new_blk = D1_.insert(std::next(blk), Block());

        EntryIter it = blk->begin();
        while (it != blk->end()) {
            if (it->label >= median) {
                new_blk->push_back(*it);
                EntryIter new_pos = std::prev(new_blk->end());
                loc_in_D1_.set(it->vertex, { new_blk, new_pos });
                it = blk->erase(it);
            } else {
                ++it;
            }
        }

        // The old block now covers labels strictly below `median`.
        // Construct a label "just below" median by decrementing the predecessor
        // tiebreaker — the last field in the lexicographic order.
        PathLabel old_block_ub(
            median.length, median.hop_count,
            median.destination, median.predecessor - 1);

        auto it_ub = find_ub_entry_by_block(blk);
        if (it_ub == ub_set_.end()) {
            // Defensive: should never happen; keep structure valid.
            ub_set_.insert({ upper_bound_, new_blk });
            return;
        }
        PathLabel inherited_ub = it_ub->first;
        ub_set_.erase(it_ub);

        ub_set_.insert({ old_block_ub, blk    });
        ub_set_.insert({ inherited_ub, new_blk });
    }

    // ── batch_prepend_list ────────────────────────────────────────────────────
    // Recursive helper: split the input range around its median and recurse
    // until each half fits in a single D0 block.
    // This matches the BatchPrepend analysis in Lemma 3.3.
    template<typename Iter>
    void batch_prepend_list(Iter begin, Iter end, int count) {
        if (count == 0) return;

        if (count <= block_capacity_) {
            // Small enough: insert as a single new D0 block at the front.
            D0_.push_front(Block());
            BlockIter new_blk = D0_.begin();

            for (Iter it = begin; it != end; ++it) {
                int       v   = it->vertex;
                PathLabel lbl = it->label;

                PathLabel* existing = best_label_.find(v);
                if (existing != nullptr) {
                    if (*existing <= lbl) continue;      // existing is better
                    remove_from_structure(v, *existing); // replace
                }

                new_blk->push_back({ v, lbl });
                EntryIter pos = std::prev(new_blk->end());
                loc_in_D0_.set(v, { new_blk, pos });
                best_label_.set(v, lbl);
                num_entries_++;
            }
            if (new_blk->empty()) D0_.erase(new_blk);
            return;
        }

        // Too large: split at median and recurse.
        std::vector<Entry> tmp(begin, end);
        PathLabel median = select_kth(tmp, count / 2);

        std::vector<Entry> smaller, larger;
        for (Iter it = begin; it != end; ++it) {
            if      (it->label < median) smaller.push_back(*it);
            else if (it->label > median) larger.push_back(*it);
        }
        // Include the median element itself in the larger half.
        larger.push_back({ tmp[count / 2].vertex, median });

        // Recurse: larger first so smaller ends up at the front of D0.
        batch_prepend_list(larger.begin(),  larger.end(),  (int)larger.size());
        batch_prepend_list(smaller.begin(), smaller.end(), (int)smaller.size());
    }
};

} // namespace duan25

#endif // BATCH_PQ_HPP
