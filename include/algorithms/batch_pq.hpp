// batch_pq.hpp
// =============================================================================
// Partial-order batch priority queue, as described in Lemma 3.3 of
// Duan et al. (2025).
//
// Overview
// --------
// A standard min-heap imposes a total order on its elements, which means any
// insert or extract-min costs O(log n). The BMSSP algorithm doesn't need a
// full ordering — it only needs to pull out a batch of the smallest M elements
// at a time. This weaker requirement is what lets us escape the O(n log n)
// sorting lower bound.
//
// The structure organises elements into two sequences of fixed-size blocks:
//
//   D0  — holds elements inserted via batch_prepend (known to be smaller than
//          everything currently in D1).
//   D1  — holds elements inserted one at a time via insert.
//
// Within each block, elements have no ordering between them. Between blocks,
// however, we do maintain the invariant that all elements in an earlier block
// are smaller than all elements in a later block. A set of upper bounds (UBs)
// tracks this inter-block ordering for D1.
//
// Key operations:
//   insert       — O(log(N/M)) amortised, where N is the total number of
//                  insertions and M is the block size.
//   batch_prepend — O(L * log(L/M)) amortised for a batch of L elements.
//   pull         — returns up to M elements with the smallest labels in O(M).
//   erase        — removes a specific vertex in O(1) expected time.
//
// The block size M is set per-call in initialise() and corresponds to the
// batch size 2^{(l-1)*t} used at each recursion level of bmsspRec.
//
// References
// ----------
//   Duan et al., "Breaking the Sorting Barrier for Directed SSSP", STOC 2025.
//     https://arxiv.org/pdf/2504.17033  — Lemma 3.3, Section 3.
//   Castro et al., "Implementation Notes on Duan et al.", 2025.
//     https://arxiv.org/pdf/2511.03007  — Section 4 (engineering details).
// =============================================================================

#ifndef BATCH_PQ_HPP
#define BATCH_PQ_HPP

#include <algorithm>
#include <list>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "path_label.hpp"

namespace duan25 {

// A single entry in the queue: a vertex id and its current path label.
struct Entry {
    int       vertex;
    PathLabel label;
};

// =============================================================================
class BatchPQ {
    using Label   = PathLabel;
    using Element = std::pair<int, Label>;  // (vertex, label)
    using Block   = std::list<Element>;
    using Blocks  = std::list<Block>;
    using BlockIt = Blocks::iterator;
    using ElemIt  = Block::iterator;

    // Comparator for the upper-bound set. Pairs are (upper_bound_label, block_iterator).
    // We need a strict weak order that also distinguishes blocks with equal upper bounds,
    // so we fall back to comparing raw pointer addresses.
    struct CompareUpperBound {
        bool operator()(const std::pair<Label, BlockIt>& a,
                        const std::pair<Label, BlockIt>& b) const {
            if (a.first != b.first) return a.first < b.first;
            return std::addressof(*a.second) < std::addressof(*b.second);
        }
    };

    // Used only as a sentinel for lower_bound queries; never dereferenced.
    BlockIt sentinel_it;

    // D0: blocks from batch_prepend. These all sit below D1 by construction.
    // D1: blocks from single insert calls, maintained in sorted block order.
    Blocks D0, D1;

    // Sorted set of (upper_bound, block_iterator) pairs covering D1.
    // Each block in D1 has an upper bound — no element in the block exceeds it,
    // and the upper bound is strictly less than the minimum of the next block.
    std::set<std::pair<Label, BlockIt>, CompareUpperBound> upper_bounds;

    int   block_size;   // maximum number of elements per block (= M in the paper)
    int   num_elements; // total live elements across D0 and D1
    Label bound;        // the overall upper bound B passed to initialise()

    // Maps each live vertex to its current best label. Used to detect stale insertions
    // and to look up labels during deletion.
    std::unordered_map<int, Label> best_label;

    // Location maps: tell us which block and which element within that block
    // a vertex lives in, for O(1) deletion.
    std::unordered_map<int, std::pair<BlockIt, ElemIt>> location_in_D0;
    std::unordered_map<int, std::pair<BlockIt, ElemIt>> location_in_D1;

public:
    // Allocates the internal hash maps with enough capacity for n vertices.
    explicit BatchPQ(int n) {
        (void)n;
    }

    // Resets the queue for a new BMSSP call at some recursion level.
    // M is the batch size for this level; B is the distance upper bound.
    // Called once per bmsspRec invocation before any inserts.
    void initialise(int M, Label B) {
        block_size   = M;
        bound        = B;
        D0           = {};
        D1           = { Block() };
        upper_bounds = { std::make_pair(B, D1.begin()) };
        num_elements = 0;

        best_label.clear();
        location_in_D0.clear();
        location_in_D1.clear();
    }

    // Returns the number of live elements currently in the queue.
    int size() const {
        return num_elements;
    }

    // Inserts vertex with the given label. If the vertex already exists with
    // a smaller or equal label, the call is a no-op. If it exists with a larger
    // label, the old entry is removed first and this one replaces it.
    // Amortised cost: O(log(N/M)) where N is the total number of insertions.
    void insert(int vertex, Label label) {
        auto existing = best_label.find(vertex);
        bool found    = (existing != best_label.end());

        if (found && existing->second > label) {
            // The incoming label is an improvement — remove the stale entry.
            remove_vertex(label);
        } else if (found) {
            // Already present with an equal or better label; nothing to do.
            return;
        }

        // Find the first D1 block whose upper bound is >= the new label,
        // then append the element to that block.
        auto it_bound_block = upper_bounds.lower_bound({ label, sentinel_it });
        auto [ub, it_block] = (*it_bound_block);

        ElemIt it = it_block->insert(it_block->end(), { vertex, label });
        location_in_D1[vertex] = { it_block, it };
        best_label[vertex]     = label;
        num_elements++;

        // Split the block if it has grown beyond the allowed size.
        if ((int)it_block->size() > block_size) {
            split_block(it_block);
        }
    }

    // Bulk-inserts a set of entries that are known to be smaller than everything
    // currently in D1. They go into D0 and will be returned by pull() first.
    // Amortised cost: O(|entries| * log(|entries|/M)).
    void batch_prepend(const std::vector<Entry>& entries) {
        std::vector<Label> labels;
        labels.reserve(entries.size());
        for (const auto& e : entries) labels.push_back(e.label);
        prepend_labels(labels);
    }

    // Extracts up to block_size elements with the globally smallest labels.
    // Returns a pair: (separator_label, list_of_vertices), where separator_label
    // is a value strictly greater than every returned element and <= the smallest
    // remaining element (or bound if the queue is now empty).
    // Cost: O(block_size).
    std::pair<Label, std::vector<int>> pull() {
        auto from_D0 = collect_prefix(D0);
        auto from_D1 = collect_prefix(D1);

        // If the total fits within one batch, return everything.
        if ((int)(from_D0.size() + from_D1.size()) <= block_size) {
            std::vector<int> result = drain_selected(from_D0, from_D1);
            return { bound, result };
        }

        // Otherwise we need to select the smallest block_size elements from
        // the combined pool using a linear-time nth_element selection.
        auto& combined = from_D0;
        combined.insert(combined.end(), from_D1.begin(), from_D1.end());

        Label median = select_kth(combined, block_size);
        std::vector<int> result = drain_below(combined, median);
        return { median, result };
    }

    // Removes a specific vertex from the queue if it is present.
    // Used by bmsspRec to clean up vertices that were completed via
    // Bellman-Ford in find_pivots and no longer need to be in the queue.
    void erase(int key) {
        if (best_label.find(key) != best_label.end()) {
            // We only need the destination field to identify the vertex;
            // the other fields are filled with dummies.
            remove_vertex(Label{ -1, -1, key, -1 });
        }
    }

private:

    // Removes the entry for the vertex identified by label.destination.
    // Checks D1 first (more common), then D0.
    void remove_vertex(Label label) {
        int vertex = label.destination;
        Label current_label = best_label[vertex];

        auto it_in_D1 = location_in_D1.find(vertex);
        if (it_in_D1 != location_in_D1.end()) {
            auto [it_block, it_elem] = it_in_D1->second;

            it_block->erase(it_elem);
            location_in_D1.erase(vertex);

            // If the block is now empty, remove it (unless it's the sentinel
            // block that carries the global upper bound).
            if (it_block->empty()) {
                auto it_ub = upper_bounds.lower_bound({ current_label, it_block });
                if (it_ub->first != bound) {
                    upper_bounds.erase(it_ub);
                    D1.erase(it_block);
                }
            }
        } else {
            auto [it_block, it_elem] = location_in_D0[vertex];
            it_block->erase(it_elem);
            location_in_D0.erase(vertex);
            if (it_block->empty()) D0.erase(it_block);
        }

        best_label.erase(vertex);
        num_elements--;
    }

    // Linear-time selection: partially sorts v so that v[k] is the element
    // that would appear at position k in a fully sorted sequence.
    // Returns the label at position k after partitioning.
    Label select_kth(std::vector<Element>& v, int k) {
        const auto by_label = [](const auto& a, const auto& b) {
            return a.second < b.second;
        };
        std::nth_element(v.begin(), v.begin() + k, v.end(), by_label);
        return v[k].second;
    }

    // Splits a D1 block that has exceeded block_size into two halves.
    // The lower half stays in the original block; the upper half moves to
    // a new block inserted immediately after. The upper_bounds set is updated
    // to reflect both new blocks.
    void split_block(BlockIt it_block) {
        int block_count = (int)it_block->size();

        std::vector<Element> elements(it_block->begin(), it_block->end());
        Label median = select_kth(elements, block_count / 2);

        // Insert a new block right after the current one for the upper half.
        auto pos_after   = it_block;
        pos_after++;
        auto new_block = D1.insert(pos_after, Block());

        // Walk the original block and move all elements >= median to new_block.
        auto it_elem = it_block->begin();
        while (it_elem != it_block->end()) {
            if (it_elem->second >= median) {
                new_block->push_back(std::move(*it_elem));
                auto it_new_elem = new_block->end();
                it_new_elem--;
                location_in_D1[it_elem->first] = { new_block, it_new_elem };
                it_elem = it_block->erase(it_elem);
            } else {
                it_elem++;
            }
        }

        // Update the upper_bounds set: the old entry is replaced by two new ones,
        // one capping the lower half just below median, one capping the upper half
        // at whatever the old upper bound was.
        Label lower_cap = { median.length, median.hop_count,
                            median.destination, median.predecessor - 1 };

        auto it_old_ub = upper_bounds.lower_bound({ lower_cap, sentinel_it });
        auto [old_ub, aux_it] = (*it_old_ub);

        upper_bounds.insert({ lower_cap, it_block });
        upper_bounds.insert({ old_ub,    new_block });
        upper_bounds.erase(it_old_ub);
    }

    // Recursively builds D0 blocks from a list of labels known to be smaller
    // than everything in D1. If the list fits in one block it is inserted directly;
    // otherwise it is split at the median and each half is prepended separately
    // (smaller half last, so it ends up at the front).
    void prepend_labels(const std::vector<Label>& labels) {
        std::list<Element> elements;
        std::unordered_map<int, Label> best_local;
        for (const auto& lbl : labels) {
            auto it = best_local.find(lbl.destination);
            if (it == best_local.end() || lbl < it->second) {
                best_local[lbl.destination] = lbl;
            }
        }

        for (const auto& [vertex, lbl] : best_local) {
            elements.push_back({ vertex, lbl });
        }
        prepend_elements(elements);
    }

    void prepend_elements(const std::list<Element>& elements) {
        std::vector<std::list<Element>> pending;
        pending.push_back(elements);

        while (!pending.empty()) {
            std::list<Element> current = std::move(pending.back());
            pending.pop_back();

            int count = (int)current.size();
            if (count == 0) continue;

            if (count <= block_size) {
                insert_prepend_block(current);
                continue;
            }

            std::vector<Element> as_vec(current.begin(), current.end());
            Label median = select_kth(as_vec, count / 2);

            std::list<Element> lower_half, upper_half;
            for (const auto& [v, lbl] : current) {
                if (lbl < median) {
                    lower_half.push_back({ v, lbl });
                } else if (lbl > median) {
                    upper_half.push_back({ v, lbl });
                }
            }
            upper_half.push_back({ median.destination, median });

            pending.push_back(std::move(lower_half));
            pending.push_back(std::move(upper_half));
        }
    }

    std::vector<Element> collect_prefix(const Blocks& blocks) {
        std::vector<Element> collected;
        auto it_block = blocks.begin();
        while (it_block != blocks.end() && (int)collected.size() <= block_size) {
            for (const auto& x : *it_block) collected.push_back(x);
            ++it_block;
        }
        return collected;
    }

    std::vector<int> drain_selected(const std::vector<Element>& from_D0,
                                    const std::vector<Element>& from_D1) {
        std::vector<int> result;
        for (const auto& [v, lbl] : from_D0) {
            result.push_back(v);
            remove_vertex(lbl);
        }
        for (const auto& [v, lbl] : from_D1) {
            result.push_back(v);
            remove_vertex(lbl);
        }
        return result;
    }

    std::vector<int> drain_below(const std::vector<Element>& combined, const Label& cut) {
        std::vector<int> result;
        for (const auto& [v, lbl] : combined) {
            if (lbl < cut) {
                result.push_back(v);
                remove_vertex(lbl);
            }
        }
        return result;
    }

    void insert_prepend_block(const std::list<Element>& elements) {
        D0.push_front(Block());
        auto new_block = D0.begin();

        for (const auto& elem : elements) {
            auto existing = best_label.find(elem.first);
            bool found    = (existing != best_label.end());

            if (found && existing->second > elem.second) {
                remove_vertex(elem.second);
            } else if (found) {
                continue;
            }

            new_block->push_back(elem);
            auto it_new_elem = new_block->end();
            --it_new_elem;
            location_in_D0[elem.first] = { new_block, it_new_elem };
            best_label[elem.first]     = elem.second;
            num_elements++;
        }

        if (new_block->empty()) D0.erase(new_block);
    }
};

} // namespace duan25

#endif // BATCH_PQ_HPP