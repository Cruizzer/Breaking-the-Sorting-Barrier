// Implements the "path label" concept from Section 2 of Duan et al. (2025).
//
// Background — Assumption 2.1 (Total order of paths)
// ---------------------------------------------------
// The correctness of Algorithm 3 (BMSSP) relies on every path from the source
// having a 'unique' length.  Real-weighted graphs do not guarantee this in
// general, so the paper augments each distance estimate with extra fields that
// break ties deterministically.
//
// A path label is the tuple:
//
//     ( length, hop_count, destination, predecessor )
//
// and paths are compared lexicographically on this tuple (Section 2, "Total
// order of Paths").  The paper notes that O(1)-time comparison is achievable
// because, when lengths and hop counts are equal, it suffices to compare just
// the destination vertices (or the predecessor vertices when the destinations
// are also equal).  All four fields are stored together here for simplicity.
//
// Floating-point rounding
// -----------------------
// Accumulated additions on double-precision values can produce tiny rounding
// differences between paths that are arithmetically identical.  To prevent
// such noise from creating spurious inequalities, the length field is rounded
// to a fixed number of decimal places on construction.  The scale factor (1e9)
// gives nine decimal places of precision, which is sufficient for most
// real-world weight ranges without distorting the ordering.
// =============================================================================

#ifndef PATH_LABEL_HPP
#define PATH_LABEL_HPP

#include <cmath>
#include <limits>
#include <tuple>

namespace duan25 {

// Precision constant used to round accumulated floating-point lengths.
// Rounding to 1/ROUND_SCALE decimal places suppresses sub-epsilon noise
// without affecting the correctness of distance comparisons.
static const double ROUND_SCALE = 1e9;

// A PathLabel uniquely identifies a shortest-path estimate for a single vertex.
// It is compared lexicographically: shorter length first, then fewer hops, then
// smaller destination index, then smaller predecessor index.
struct PathLabel {
    double length;       // accumulated edge-weight sum (rounded)
    int    hop_count;    // number of edges on this path
    int    destination;  // the vertex this label belongs to
    int    predecessor;  // the previous vertex on the path

    // Default-initialised to a "null" state (used by data-structure internals).
    PathLabel()
        : length(0.0), hop_count(0), destination(0), predecessor(0) {}

    PathLabel(double len, int hops, int dest, int pred)
        : length(
            (!std::isfinite(len) || std::fabs(len) > std::numeric_limits<double>::max() / ROUND_SCALE)
                ? len
                : std::round(len * ROUND_SCALE) / ROUND_SCALE)
        , hop_count(hops)
        , destination(dest)
        , predecessor(pred)
    {}

    // Lexicographic less-than, as required by Assumption 2.1.
    bool operator<(const PathLabel& other) const {
        if (length != other.length)         return length      < other.length;
        if (hop_count != other.hop_count)   return hop_count   < other.hop_count;
        if (destination != other.destination) return destination < other.destination;
        return predecessor < other.predecessor;
    }

    bool operator<=(const PathLabel& other) const { return !(other < *this); }
    bool operator> (const PathLabel& other) const { return other < *this;    }
    bool operator>=(const PathLabel& other) const { return !(*this < other); }

    bool operator==(const PathLabel& other) const {
        return length      == other.length
            && hop_count   == other.hop_count
            && destination == other.destination
            && predecessor == other.predecessor;
    }

    bool operator!=(const PathLabel& other) const { return !(*this == other); }
};

// A sentinel label representing "infinite distance" — used to initialise
// distance estimates before any path is found.
inline PathLabel infinite_label() {
    // We use max/10 rather than max to avoid overflow when adding edge weights.
    double inf = std::numeric_limits<double>::max() / 10.0;
    return PathLabel(inf, 0, 0, 0);
}

} // namespace duan25

#endif // PATH_LABEL_HPP
