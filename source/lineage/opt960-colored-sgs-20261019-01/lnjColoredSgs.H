#ifndef LNJ_COLORED_SGS_H
#define LNJ_COLORED_SGS_H

// Standalone prototype: no OpenFOAM, MPI, or CPE dependencies.
// The permutation changes the Gauss-Seidel ordering; this is not natural-order GS.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace lnj
{
enum class ColoredSgsStructure
{
    strictBidirectional,
    // Caller guarantees reverse edges: e.g. OpenFOAM CSR generated from LDU.
    knownBidirectional
};

template<class Label>
std::size_t coloredSgsIndex(const Label value)
{
    static_assert(std::is_integral<Label>::value, "Label must be integral");
    static_assert(!std::is_same<Label, bool>::value, "bool is not an index");
    if constexpr (std::is_signed<Label>::value)
    {
        if (value < 0) throw std::invalid_argument("negative CSR index");
    }
    const auto unsignedValue = static_cast<std::make_unsigned_t<Label>>(value);
    if constexpr (sizeof(Label) > sizeof(std::size_t))
    {
        if (unsignedValue > std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("CSR index exceeds size_t");
    }
    return static_cast<std::size_t>(unsignedValue);
}

template<class Label>
Label coloredSgsLabel(const std::size_t value)
{
    static_assert(std::is_integral<Label>::value, "Label must be integral");
    if (value > static_cast<std::size_t>(std::numeric_limits<Label>::max()))
        throw std::overflow_error("CSR size exceeds Label range");
    return static_cast<Label>(value);
}

template<class Label, class Scalar = double>
struct ColoredSgs
{
    // Both directions are explicit: new = originalToPermuted[old].
    std::vector<Label> originalToPermuted;
    std::vector<Label> permutedToOriginal;
    // Color c occupies [colorStarts[c], colorStarts[c+1]). Empty n => {0}.
    std::vector<Label> colorStarts;
    // CSR contains off-diagonal entries only. Duplicate entries are preserved.
    std::vector<Label> rowPtr;
    std::vector<Label> columns;
    std::vector<Scalar> values;
    std::vector<Scalar> diagonal;

    std::size_t size() const { return diagonal.size(); }
    std::size_t colorCount() const { return colorStarts.size() - 1; }

    std::vector<Scalar> pack(const std::vector<Scalar>& original) const
    {
        if (original.size() != size())
            throw std::invalid_argument("pack vector size mismatch");
        std::vector<Scalar> result(size());
        for (std::size_t i = 0; i < size(); ++i)
            result[coloredSgsIndex(originalToPermuted[i])] = original[i];
        return result;
    }

    std::vector<Scalar> unpack(const std::vector<Scalar>& permuted) const
    {
        if (permuted.size() != size())
            throw std::invalid_argument("unpack vector size mismatch");
        std::vector<Scalar> result(size());
        for (std::size_t i = 0; i < size(); ++i)
            result[coloredSgsIndex(permutedToOriginal[i])] = permuted[i];
        return result;
    }

    // CPU oracle for an eventual CPE implementation: one independent batch
    // per color, forward colors then reverse colors. x and rhs are permuted.
    // Each batch is contiguous, so the CPE implementation can partition row
    // intervals and DMA whole output blocks rather than issue one DMA per row.
    // MPI halo updates / interface contributions are intentionally not modeled.
    void smooth
    (
        std::vector<Scalar>& x,
        const std::vector<Scalar>& rhs,
        const std::size_t sweeps = 1
    ) const
    {
        if (x.size() != size() || rhs.size() != size())
            throw std::invalid_argument("smooth vector size mismatch");
        if (&x == &rhs)
            throw std::invalid_argument("solution and rhs must not alias");
        for (std::size_t i = 0; i < size(); ++i)
        {
            if (!std::isfinite(x[i]) || !std::isfinite(rhs[i]))
                throw std::invalid_argument("smooth requires finite vectors");
        }
        const auto batch = [&](const std::size_t color)
        {
            const auto begin = coloredSgsIndex(colorStarts[color]);
            const auto end = coloredSgsIndex(colorStarts[color + 1]);
            for (std::size_t row = begin; row < end; ++row)
            {
                Scalar sum = rhs[row];
                for (std::size_t k = coloredSgsIndex(rowPtr[row]);
                     k < coloredSgsIndex(rowPtr[row + 1]); ++k)
                    sum -= values[k] * x[coloredSgsIndex(columns[k])];
                const Scalar value = sum / diagonal[row];
                if (!std::isfinite(value))
                    throw std::overflow_error("non-finite SGS result");
                x[row] = value;
            }
        };
        for (std::size_t sweep = 0; sweep < sweeps; ++sweep)
        {
            for (std::size_t c = 0; c < colorCount(); ++c) batch(c);
            for (std::size_t c = colorCount(); c > 0; --c) batch(c - 1);
        }
    }
};

template<class Label, class Scalar>
ColoredSgs<Label, Scalar> makeColoredSgs
(
    const std::vector<Label>& rowPtr,
    const std::vector<Label>& columns,
    const std::vector<Scalar>& values,
    const std::vector<Scalar>& diagonal,
    const ColoredSgsStructure structure = ColoredSgsStructure::strictBidirectional
)
{
    static_assert(std::is_floating_point<Scalar>::value,
                  "Scalar must be floating point");
    const auto n = diagonal.size();
    const auto nnz = columns.size();
    (void)coloredSgsLabel<Label>(n);
    (void)coloredSgsLabel<Label>(nnz);
    if (rowPtr.size() != n + 1 || values.size() != nnz)
        throw std::invalid_argument("CSR array length mismatch");
    if (coloredSgsIndex(rowPtr.front()) != 0
        || coloredSgsIndex(rowPtr.back()) != nnz)
        throw std::invalid_argument("invalid CSR endpoints");

    std::map<std::pair<std::size_t, std::size_t>, std::size_t> edgeCounts;
    for (std::size_t row = 0; row < n; ++row)
    {
        const auto first = coloredSgsIndex(rowPtr[row]);
        const auto last = coloredSgsIndex(rowPtr[row + 1]);
        if (first > last || last > nnz)
            throw std::invalid_argument("CSR row pointers not monotone/bounded");
        if (!std::isfinite(diagonal[row]) || diagonal[row] == Scalar(0))
            throw std::invalid_argument("diagonal must be finite and nonzero");
        for (std::size_t k = first; k < last; ++k)
        {
            const auto col = coloredSgsIndex(columns[k]);
            if (col >= n || col == row)
                throw std::invalid_argument("invalid off-diagonal column");
            if (!std::isfinite(values[k]))
                throw std::invalid_argument("matrix values must be finite");
            if (structure == ColoredSgsStructure::strictBidirectional)
                ++edgeCounts[{row, col}];
        }
    }
    // Non-symmetric VALUES are allowed, but every face must have both directed
    // entries. Repeated faces are accepted with matching reverse multiplicity.
    for (const auto& entry : edgeCounts)
    {
        const auto reverse = edgeCounts.find({entry.first.second, entry.first.first});
        if (reverse == edgeCounts.end() || reverse->second != entry.second)
            throw std::invalid_argument("CSR structure must be symmetric");
    }

    const auto uncolored = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> colors(n, uncolored);
    std::vector<std::size_t> seen(n, uncolored);
    std::size_t colorCount = 0;
    // Deterministic greedy coloring in original row order, choosing the first
    // available color. No assumption on input column ordering is needed.
    for (std::size_t row = 0; row < n; ++row)
    {
        for (std::size_t k = coloredSgsIndex(rowPtr[row]);
             k < coloredSgsIndex(rowPtr[row + 1]); ++k)
        {
            const auto neighborColor = colors[coloredSgsIndex(columns[k])];
            if (neighborColor != uncolored) seen[neighborColor] = row;
        }
        std::size_t color = 0;
        while (color < n && seen[color] == row) ++color;
        if (color == n) throw std::logic_error("greedy coloring failed");
        colors[row] = color;
        colorCount = std::max(colorCount, color + 1);
    }
    // Always verify the actual parallel-safety condition, including on the
    // trusted bidirectional fast path. A one-sided CSR is not promised to be
    // detected by this path: the caller still owns the bidirectionality proof.
    // Even for malformed one-sided input, no same-color dependency is allowed.
    for (std::size_t row = 0; row < n; ++row)
        for (std::size_t k = coloredSgsIndex(rowPtr[row]);
             k < coloredSgsIndex(rowPtr[row + 1]); ++k)
            if (colors[row] == colors[coloredSgsIndex(columns[k])])
                throw std::invalid_argument("same-color dependency in CSR");

    ColoredSgs<Label, Scalar> result;
    std::vector<std::size_t> offsets(colorCount + 1, 0);
    for (const auto color : colors) ++offsets[color + 1];
    for (std::size_t c = 0; c < colorCount; ++c) offsets[c + 1] += offsets[c];
    for (const auto offset : offsets)
        result.colorStarts.push_back(coloredSgsLabel<Label>(offset));
    auto next = offsets;
    result.originalToPermuted.resize(n);
    result.permutedToOriginal.resize(n);
    for (std::size_t row = 0; row < n; ++row)
    {
        const auto newRow = next[colors[row]]++;
        result.originalToPermuted[row] = coloredSgsLabel<Label>(newRow);
        result.permutedToOriginal[newRow] = coloredSgsLabel<Label>(row);
    }

    result.rowPtr.push_back(Label(0));
    result.columns.reserve(nnz);
    result.values.reserve(nnz);
    result.diagonal.reserve(n);
    for (std::size_t row = 0; row < n; ++row)
    {
        const auto oldRow = coloredSgsIndex(result.permutedToOriginal[row]);
        result.diagonal.push_back(diagonal[oldRow]);
        for (std::size_t k = coloredSgsIndex(rowPtr[oldRow]);
             k < coloredSgsIndex(rowPtr[oldRow + 1]); ++k)
        {
            result.columns.push_back(result.originalToPermuted[coloredSgsIndex(columns[k])]);
            result.values.push_back(values[k]);
        }
        result.rowPtr.push_back(coloredSgsLabel<Label>(result.values.size()));
    }
    return result;
}

// Explicit trust-boundary API. Unlike makeColoredSgs's strict default, this
// omits reverse-edge/multiplicity map construction. All numeric/index checks
// and a full linear no-same-color-edge check remain enabled.
template<class Label, class Scalar>
ColoredSgs<Label, Scalar> makeColoredSgsKnownBidirectional
(
    const std::vector<Label>& rowPtr,
    const std::vector<Label>& columns,
    const std::vector<Scalar>& values,
    const std::vector<Scalar>& diagonal
)
{
    return makeColoredSgs(rowPtr, columns, values, diagonal,
                          ColoredSgsStructure::knownBidirectional);
}
} // namespace lnj
#endif
