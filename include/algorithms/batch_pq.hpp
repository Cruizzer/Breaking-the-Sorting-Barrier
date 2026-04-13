#ifndef BATCH_PQ_HPP
#define BATCH_PQ_HPP

#include <algorithm>
#include <list>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "path_label.hpp"

namespace duan25 {

struct Entry {
    int vertex;
    PathLabel label;
};

class BatchPQ {
    using Label = PathLabel;
    using Element = std::pair<int, Label>;
    using Block = std::list<Element>;
    using Blocks = std::list<Block>;
    using BlockIter = Blocks::iterator;
    using ElemIter = Block::iterator;

    struct CompareUB {
        bool operator()(const std::pair<Label, BlockIter>& a,
                        const std::pair<Label, BlockIter>& b) const {
            if (a.first != b.first) return a.first < b.first;
            return std::addressof(*a.second) < std::addressof(*b.second);
        }
    };

    BlockIter it_min;
    Blocks D0, D1;
    std::set<std::pair<Label, BlockIter>, CompareUB> UBs;

    int M = 0;
    int size_ = 0;
    Label B;

    std::unordered_map<int, Label> actual_value;
    std::unordered_map<int, std::pair<BlockIter, ElemIter>> where_is0, where_is1;

public:
    explicit BatchPQ(int n) {
        actual_value.reserve(n);
        where_is0.reserve(n);
        where_is1.reserve(n);
    }

    void initialise(int M_, Label B_) {
        M = M_;
        B = B_;
        D0 = {};
        D1 = {Block()};
        UBs = {std::make_pair(B_, D1.begin())};
        size_ = 0;
        actual_value.clear();
        where_is0.clear();
        where_is1.clear();
    }

    int size() const {
        return size_;
    }

    void insert(int vertex, Label label) {
        int a = vertex;
        Label b = label;

        auto it_exist = actual_value.find(a);
        int exist = (it_exist != actual_value.end());

        if (exist && it_exist->second > b) {
            delete_(b);
        } else if (exist) {
            return;
        }

        auto it_UB_block = UBs.lower_bound({b, it_min});
        auto [ub, it_block] = (*it_UB_block);

        auto it = it_block->insert(it_block->end(), {a, b});
        where_is1[a] = {it_block, it};
        actual_value[a] = b;

        size_++;

        if ((int)(*it_block).size() > M) {
            split(it_block);
        }
    }

    void batch_prepend(const std::vector<Entry>& entries) {
        std::vector<Label> labels;
        labels.reserve(entries.size());
        for (const auto& e : entries) labels.push_back(e.label);
        batchPrepend(labels);
    }

    std::pair<Label, std::vector<int>> pull() {
        std::vector<Element> s0, s1;
        s0.reserve(2 * M);
        s1.reserve(M);

        auto it_block = D0.begin();
        while (it_block != D0.end() && (int)s0.size() <= M) {
            for (const auto& x : *it_block) s0.push_back(x);
            ++it_block;
        }

        it_block = D1.begin();
        while (it_block != D1.end() && (int)s1.size() <= M) {
            for (const auto& x : *it_block) s1.push_back(x);
            ++it_block;
        }

        if ((int)(s1.size() + s0.size()) <= M) {
            std::vector<int> ret;
            ret.reserve(s1.size() + s0.size());
            for (auto [a, b] : s0) {
                ret.push_back(a);
                delete_(b);
            }
            for (auto [a, b] : s1) {
                ret.push_back(a);
                delete_(b);
            }
            return {B, ret};
        }

        auto& l = s0;
        l.insert(l.end(), s1.begin(), s1.end());

        Label med = selectKth(l, M);
        std::vector<int> ret;
        ret.reserve(M);
        for (auto [a, b] : l) {
            if (b < med) {
                ret.push_back(a);
                delete_(b);
            }
        }
        return {med, ret};
    }

    void erase(int key) {
        if (actual_value.find(key) != actual_value.end()) {
            delete_(Label{-1, -1, key, -1});
        }
    }

private:
    void delete_(Label x) {
        int a = x.destination;
        Label b = actual_value[a];

        auto it_w = where_is1.find(a);
        if (it_w != where_is1.end()) {
            auto [it_block, it] = it_w->second;
            (*it_block).erase(it);
            where_is1.erase(a);

            if ((int)(*it_block).size() == 0) {
                auto it_UB_block = UBs.lower_bound({b, it_block});
                if ((*it_UB_block).first != B) {
                    UBs.erase(it_UB_block);
                    D1.erase(it_block);
                }
            }
        } else {
            auto [it_block, it] = where_is0[a];
            (*it_block).erase(it);
            where_is0.erase(a);
            if ((int)(*it_block).size() == 0) D0.erase(it_block);
        }

        actual_value.erase(a);
        size_--;
    }

    Label selectKth(std::vector<Element>& v, int k) {
        const auto comparator = [](const auto& a, const auto& b) {
            return a.second < b.second;
        };
        std::nth_element(v.begin(), v.begin() + k, v.end(), comparator);
        return v[k].second;
    }

    void split(BlockIter it_block) {
        int sz = (int)(*it_block).size();

        std::vector<Element> v((*it_block).begin(), (*it_block).end());
        Label med = selectKth(v, sz / 2);

        auto pos = it_block;
        ++pos;
        auto new_block = D1.insert(pos, Block());
        auto it = (*it_block).begin();

        while (it != (*it_block).end()) {
            if ((*it).second >= med) {
                (*new_block).push_back(std::move(*it));
                auto it_new = (*new_block).end();
                --it_new;
                where_is1[(*it).first] = {new_block, it_new};
                it = (*it_block).erase(it);
            } else {
                ++it;
            }
        }

        Label UB1 = {med.length, med.hop_count, med.destination, med.predecessor - 1};
        auto it_lb = UBs.lower_bound({UB1, it_min});
        auto [UB2, aux] = (*it_lb);

        UBs.insert({UB1, it_block});
        UBs.insert({UB2, new_block});
        UBs.erase(it_lb);
    }

    void batchPrepend(const std::vector<Label>& labels) {
        std::list<Element> l;
        for (const auto& x : labels) {
            l.push_back({x.destination, x});
        }
        batchPrepend(l);
    }

    void batchPrepend(const std::list<Element>& l) {
        int sz = (int)l.size();
        if (sz == 0) return;

        if (sz <= M) {
            D0.push_front(Block());
            auto new_block = D0.begin();

            for (const auto& x : l) {
                auto it = actual_value.find(x.first);
                int exist = (it != actual_value.end());

                if (exist && it->second > x.second) {
                    delete_(x.second);
                } else if (exist) {
                    continue;
                }

                (*new_block).push_back(x);
                auto it_new = (*new_block).end();
                --it_new;
                where_is0[x.first] = {new_block, it_new};
                actual_value[x.first] = x.second;
                size_++;
            }
            if (new_block->size() == 0) D0.erase(new_block);
            return;
        }

        std::vector<Element> v(l.begin(), l.end());
        Label med = selectKth(v, sz / 2);

        std::list<Element> less, great;
        for (auto [a, b] : l) {
            if (b < med) {
                less.push_back({a, b});
            } else if (b > med) {
                great.push_back({a, b});
            }
        }
        great.push_back({med.destination, med});

        batchPrepend(great);
        batchPrepend(less);
    }
};

} // namespace duan25

#endif // BATCH_PQ_HPP
