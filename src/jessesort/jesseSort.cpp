#include "jesseSort.hpp"
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <cstring>


static void mergeRuns(
    std::vector<int>& arr,
    size_t left,
    size_t mid,
    size_t right,
    std::vector<int>& temp
) {
    size_t left_len  = mid - left;
    size_t right_len = right - mid;

    // Copy smaller side into temp
    if (left_len <= right_len) {
        std::memcpy(temp.data(), &arr[left], left_len * sizeof(int));

        size_t i = 0;
        size_t j = mid;
        size_t k = left;

        while (i < left_len && j < right) {
            if (temp[i] <= arr[j])
                arr[k++] = temp[i++];
            else
                arr[k++] = arr[j++];
        }

        if (i < left_len)
            std::memcpy(&arr[k], &temp[i], (left_len - i) * sizeof(int));
    }
    else {
        std::memcpy(temp.data(), &arr[mid], right_len * sizeof(int));

        ptrdiff_t i = static_cast<ptrdiff_t>(mid) - 1;
        ptrdiff_t j = static_cast<ptrdiff_t>(right_len) - 1;
        ptrdiff_t k = static_cast<ptrdiff_t>(right) - 1;
        ptrdiff_t left_bound = static_cast<ptrdiff_t>(left);

        while (j >= 0 && i >= left_bound) {
            if (arr[i] > temp[j])
                arr[k--] = arr[i--];
            else
                arr[k--] = temp[j--];
        }

        if (j >= 0)
            std::memcpy(&arr[left], &temp[0], (j + 1) * sizeof(int));
    }
}

std::vector<int>& best2of3(
    std::vector<int>& arr,
    const std::vector<size_t>& run_starts,
    const std::vector<size_t>& run_lengths
) {
    struct Run {
        size_t start;
        size_t length;
    };

    std::vector<Run> stack;
    stack.reserve(run_starts.size());

    std::vector<int> temp(arr.size() / 2);

    auto mergeAt = [&](size_t i) {
        Run& A = stack[i];
        Run& B = stack[i + 1];

        mergeRuns(
            arr,
            A.start,
            B.start,
            B.start + B.length,
            temp
        );

        A.length += B.length;
        stack.erase(stack.begin() + i + 1);
    };

    for (size_t i = 0; i < run_starts.size(); ++i) {
        stack.push_back({ run_starts[i], run_lengths[i] });

        // Enforce TimSort-style invariants
        while (stack.size() >= 3) {
            size_t n = stack.size();

            size_t A = stack[n-3].length;
            size_t B = stack[n-2].length;
            size_t C = stack[n-1].length;

            if (A <= B + C || B <= C) {
                if (A < C)
                    mergeAt(n-3);
                else
                    mergeAt(n-2);
            } else {
                break;
            }
        }
    }

    while (stack.size() > 1) {
        mergeAt(stack.size() - 2);
    }

    return arr;
}

void reserve_structure(std::vector<std::vector<int>>& buckets, size_t n) {
    constexpr double SQRT8 = 2.8284271247461903;

    size_t sqrt_n = static_cast<size_t>(std::sqrt(static_cast<double>(n)));

    double buffer = 1.1;
    size_t num_games = 2; // Each game requires less space
    size_t outer = static_cast<size_t>(buffer * (SQRT8 * sqrt_n) / num_games);
    size_t inner = static_cast<size_t>(buffer * (sqrt_n / SQRT8) / num_games);

    buckets.clear();
    buckets.reserve(outer);

    for (size_t i = 0; i < outer; ++i) {
        buckets.emplace_back();
        buckets.back().reserve(inner);
    }
}

// Sort 32 elements starting at index `start`
//template<typename T>
void insertion_sort_32(std::vector<int>& v, int start) {
    constexpr int BLOCK_SIZE = 32;

    for (int i = start + 1; i < start + BLOCK_SIZE; ++i) {
        int key = v[i];
        int j = i;

        while (j > start && v[j - 1] > key) {
            v[j] = v[j - 1];
            --j;
        }

        v[j] = key;
    }
}

void insertion_sort_32s(int* base) {
    // Put smallest at index 0
    for (int i = 31; i > 0; --i) {
        if (base[i] < base[i - 1])
            std::swap(base[i], base[i - 1]);
    }

    for (int i = 2; i < 32; ++i) {
        int key = base[i];
        int j = i;

        while (base[j - 1] > key) {
            base[j] = base[j - 1];
            --j;
        }

        base[j] = key;
    }
}

static inline void cmp_swap(int& a, int& b) {
    int minv = std::min(a, b);
    int maxv = std::max(a, b);
    a = minv;
    b = maxv;
}

// static inline void cmp_swap(int& a, int& b) {
//     int diff = b - a;
//     int mask = diff >> 31;
//     int minv = b + (diff & mask);
//     int maxv = a - (diff & mask);
//     a = minv;
//     b = maxv;
// }

void sort4_branchless(int& a, int& b, int& c, int& d) {
    cmp_swap(a, c);
    cmp_swap(b, d);
    cmp_swap(a, b);
    cmp_swap(c, d);
    cmp_swap(b, c);
}

void sort8_branchless(int& a, int& b, int& c, int& d, int& e, int& f, int& g, int& h) {
    // Stage 1
    cmp_swap(a, b);
    cmp_swap(c, d);
    cmp_swap(e, f);
    cmp_swap(g, h);

    // Stage 2
    cmp_swap(a, c);
    cmp_swap(b, d);
    cmp_swap(e, g);
    cmp_swap(f, h);

    // Stage 3
    cmp_swap(b, c);
    cmp_swap(f, g);
    cmp_swap(a, e);
    cmp_swap(d, h);

    // Stage 4
    cmp_swap(b, f);
    cmp_swap(c, g);

    // Stage 5
    cmp_swap(b, e);
    cmp_swap(d, g);

    // Stage 6
    cmp_swap(c, e);
    cmp_swap(d, f);

    // Stage 7
    cmp_swap(d, e);
}

void sort16_branchless(int* x) {
    #define CS(i,j) cmp_swap(x[i], x[j])

    CS(0,1); CS(2,3); CS(4,5); CS(6,7);
    CS(8,9); CS(10,11); CS(12,13); CS(14,15);

    CS(0,2); CS(1,3); CS(4,6); CS(5,7);
    CS(8,10); CS(9,11); CS(12,14); CS(13,15);

    CS(1,2); CS(5,6); CS(9,10); CS(13,14);

    CS(0,4); CS(1,5); CS(2,6); CS(3,7);
    CS(8,12); CS(9,13); CS(10,14); CS(11,15);

    CS(2,4); CS(3,5); CS(6,8); CS(7,9);
    CS(10,12); CS(11,13);

    CS(1,4); CS(3,6); CS(5,8); CS(7,10);
    CS(9,12); CS(11,14);

    CS(1,2); CS(3,4); CS(5,6); CS(7,8);
    CS(9,10); CS(11,12); CS(13,14);

    CS(2,3); CS(4,5); CS(6,7); CS(8,9);
    CS(10,11); CS(12,13);

    CS(3,4); CS(5,6); CS(7,8); CS(9,10);
    CS(11,12);

    CS(6,7); CS(8,9);

    #undef CS
}

void sort32_branchless(int* x) {
#define CS(i,j) cmp_swap(x[i], x[j])

    // Sort first 16
    sort16_branchless(x);

    // Sort second 16
    sort16_branchless(x + 16);

    // Bitonic merge 32

    for (int i = 0; i < 16; ++i) CS(i, i+16);
    for (int i = 0; i < 8; ++i)  { CS(i, i+8); CS(i+16, i+24); }
    for (int i = 0; i < 4; ++i)  {
        CS(i, i+4); CS(i+8, i+12);
        CS(i+16, i+20); CS(i+24, i+28);
    }
    for (int i = 0; i < 2; ++i)  {
        CS(i, i+2); CS(i+4, i+6);
        CS(i+8, i+10); CS(i+12, i+14);
        CS(i+16, i+18); CS(i+20, i+22);
        CS(i+24, i+26); CS(i+28, i+30);
    }
    for (int i = 0; i < 32; i += 2) CS(i, i+1);

#undef CS
}

// void sort32_branchless(int* x) {
//     #define CS(i,j) cmp_swap(x[i], x[j])

//     // Sort first 16
//     sort16_branchless(x);

//     // Sort second 16
//     sort16_branchless(x + 16);

//     // Bitonic merge 32

//     // Stage 1
//     for (int i = 0; i < 16; ++i)
//         CS(i, i + 16);

//     // Stage 2
//     for (int i = 0; i < 8; ++i) {
//         CS(i, i + 8);
//         CS(i + 16, i + 24);
//     }

//     // Stage 3
//     for (int i = 0; i < 4; ++i) {
//         CS(i, i + 4);
//         CS(i + 8, i + 12);
//         CS(i + 16, i + 20);
//         CS(i + 24, i + 28);
//     }

//     // Stage 4
//     for (int i = 0; i < 2; ++i) {
//         CS(i, i + 2);
//         CS(i + 4, i + 6);
//         CS(i + 8, i + 10);
//         CS(i + 12, i + 14);
//         CS(i + 16, i + 18);
//         CS(i + 20, i + 22);
//         CS(i + 24, i + 26);
//         CS(i + 28, i + 30);
//     }

//     // Stage 5
//     for (int i = 0; i < 32; i += 2)
//         CS(i, i + 1);

//     #undef CS
// }

// Flatten piles into arr and make all runs ascending
void vectorsToFlatArrMerge(
    std::vector<int>& arr,
    const std::vector<std::vector<int>>& pilesDescending,
    const std::vector<std::vector<int>>& pilesAscending,
    std::vector<size_t>& run_starts,
    std::vector<size_t>& run_lengths
) {
    size_t pos = 0;
    run_starts.clear();
    run_lengths.clear();

    // Merge ascending piles
    for (const auto& pile : pilesAscending) {
        run_starts.push_back(pos);
        run_lengths.push_back(pile.size());
        for (int val : pile) {
            arr[pos++] = val;
        }
    }

    // Merge descending piles, reverse each to ascending
    for (const auto& pile : pilesDescending) {
        run_starts.push_back(pos);
        run_lengths.push_back(pile.size());
        for (auto it = pile.rbegin(); it != pile.rend(); ++it) {
            arr[pos++] = *it; // reversed copy -> now ascending
        }
    }
}

static void mergeAdjacent(
    std::vector<int>& arr,
    size_t left,
    size_t mid,
    size_t right
) {
    size_t left_len  = mid - left;
    size_t right_len = right - mid;

    if (left_len <= right_len) {
        std::vector<int> buffer(left_len);
        std::memcpy(buffer.data(), &arr[left], left_len * sizeof(int));

        size_t i = 0;
        size_t j = mid;
        size_t k = left;

        while (i < left_len && j < right) {
            if (buffer[i] <= arr[j])
                arr[k++] = buffer[i++];
            else
                arr[k++] = arr[j++];
        }

        if (i < left_len)
            std::memcpy(&arr[k], &buffer[i], (left_len - i) * sizeof(int));
    }
    else {
        std::vector<int> buffer(right_len);
        std::memcpy(buffer.data(), &arr[mid], right_len * sizeof(int));

        ptrdiff_t i = static_cast<ptrdiff_t>(mid) - 1;
        ptrdiff_t j = static_cast<ptrdiff_t>(right_len) - 1;
        ptrdiff_t k = static_cast<ptrdiff_t>(right) - 1;
        ptrdiff_t left_bound = static_cast<ptrdiff_t>(left);

        while (j >= 0 && i >= left_bound) {
            if (arr[i] > buffer[j])
                arr[k--] = arr[i--];
            else
                arr[k--] = buffer[j--];
        }

        if (j >= 0) {
            std::memcpy(
                &arr[left],
                &buffer[0],
                (j + 1) * sizeof(int)
            );
        }
    }
}

// Bottom-up in-place merge of flattened runs
std::vector<int>& bottomUpMerge(
    std::vector<int>& arr,
    const std::vector<size_t>& run_starts,
    const std::vector<size_t>& run_lengths
) {
    std::vector<size_t> starts = run_starts;
    std::vector<size_t> lengths = run_lengths;

    while (starts.size() > 1) {
        std::vector<size_t> new_starts;
        std::vector<size_t> new_lengths;

        for (size_t i = 0; i + 1 < starts.size(); i += 2) {
            size_t left_start = starts[i];
            size_t left_len   = lengths[i];
            size_t right_start = starts[i+1];
            size_t right_len   = lengths[i+1];

            // Merge adjacent runs
            std::inplace_merge(
                arr.begin() + left_start,
                arr.begin() + right_start,
                arr.begin() + right_start + right_len
            );
            // mergeAdjacent(arr, left_start, right_start, right_start + right_len);

            // New merged run
            new_starts.push_back(left_start);
            new_lengths.push_back(left_len + right_len);
        }

        // If odd number of runs, carry over last one
        if (starts.size() % 2 != 0) {
            new_starts.push_back(starts.back());
            new_lengths.push_back(lengths.back());
        }

        starts = std::move(new_starts);
        lengths = std::move(new_lengths);
    }
    return arr;
}

std::vector<int>& bottomUpMergeWithTemp(
    std::vector<int>& arr,
    std::vector<size_t>& run_starts,
    std::vector<size_t>& run_lengths
) {
    if (run_starts.empty()) return arr;

    while (run_starts.size() > 1) {
        std::vector<size_t> new_starts;
        std::vector<size_t> new_lengths;

        // Compute max left run for this iteration
        size_t max_run = 0;
        for (size_t len : run_lengths) if (len > max_run) max_run = len;
        std::vector<int> temp(max_run);

        for (size_t i = 0; i + 1 < run_starts.size(); i += 2) {
            size_t left_start  = run_starts[i];
            size_t left_len    = run_lengths[i];
            size_t right_start = run_starts[i+1];
            size_t right_len   = run_lengths[i+1];

            // Skip empty runs
            if (left_len == 0 && right_len == 0) continue;
            if (left_len == 0) {
                new_starts.push_back(right_start);
                new_lengths.push_back(right_len);
                continue;
            }
            if (right_len == 0) {
                new_starts.push_back(left_start);
                new_lengths.push_back(left_len);
                continue;
            }

            // Copy left run into temp
            std::copy(arr.begin() + left_start,
                      arr.begin() + left_start + left_len,
                      temp.begin());

            // Merge back into arr
            size_t l = 0, r = right_start, dest = left_start;
            while (l < left_len && r < right_start + right_len) {
                arr[dest++] = (temp[l] <= arr[r]) ? temp[l++] : arr[r++];
            }
            while (l < left_len) arr[dest++] = temp[l++]; // leftover left run

            new_starts.push_back(left_start);
            new_lengths.push_back(left_len + right_len);
        }

        // Carry over last run if odd number
        if (run_starts.size() % 2 != 0) {
            new_starts.push_back(run_starts.back());
            new_lengths.push_back(run_lengths.back());
        }

        run_starts = std::move(new_starts);
        run_lengths = std::move(new_lengths);
    }
    return arr;
}

// // Merge two sorted std::vectors into one sorted std::vector
// // NOTE: This returns an ascending pile, reversing the order
// std::vector<int> mergeTwoDescendingPilesToAscending(std::vector<int>& pile1, std::vector<int>& pile2) {
//     std::vector<int> merged;
//     merged.reserve(pile1.size() + pile2.size());

//     // NOTE: We use int instead of size_t to let i,j go to -1, this avoids additional if-statements/breaks
//     int i = pile1.size() - 1, j = pile2.size() - 1;

//     // Merge loop: since piles are descending, we traverse them from the end (to get ascending order)
//     while (i >= 0 && j >= 0) {
//         if (pile1[i] <= pile2[j]) {
//             merged.push_back(pile1[i--]);
//         } else {
//             merged.push_back(pile2[j--]);
//         }
//     }

//     // Add remaining elements from either pile (if there are any)
//     while (i >= 0) merged.push_back(pile1[i--]);
//     while (j >= 0) merged.push_back(pile2[j--]);

//     return merged;
// }

// // Merge two sorted std::vectors into one sorted std::vector
// std::vector<int> mergeTwoAscendingPiles(std::vector<int>& pile1, std::vector<int>& pile2) {
//     std::vector<int> merged;
//     merged.reserve(pile1.size() + pile2.size());
//     size_t i = 0, j = 0;

//     // Merge loop
//     while (i < pile1.size() && j < pile2.size()) {
//         if (pile1[i] <= pile2[j]) {
//             merged.push_back(pile1[i++]);
//         } else {
//             merged.push_back(pile2[j++]);
//         }
//     }

//     // Add remaining elements
//     while (i < pile1.size()) merged.push_back(pile1[i++]);
//     while (j < pile2.size()) merged.push_back(pile2[j++]);

//     return merged;
// }

// void reverseDescendingPiles(
//     std::vector<int>& arr,
//     const std::vector<int>& pileSizes,
//     const std::vector<int>& cumsumPileIndexStarts,
//     int ascPileCount,
//     int descPileCount,
//     int descGameStartIndex
// ) {
//     const int firstDescPile = ascPileCount;
//     const int lastDescPile  = ascPileCount + descPileCount;

//     for (int p = firstDescPile; p < lastDescPile; ++p) {
//         int pileStart =
//             descGameStartIndex +
//             cumsumPileIndexStarts[p];

//         int pileSize = pileSizes[p];

//         std::reverse(
//             arr.begin() + pileStart,
//             arr.begin() + pileStart + pileSize
//         );
//     }
// }

// // Iteratively merge piles until only one pile remains, reversing to ascending order
// std::vector<int> mergeDescendingPilesToAscending(std::vector<std::vector<int>>& piles) {
//     // Check for early return
//     if (piles.empty()) {
//         return {};
//     }

//     // Pairwise merge adjacent piles
//     // NOTE: First iteration deals with reversed order
//     {
//         std::vector<std::vector<int>> newPiles;
//         newPiles.reserve(piles.size() / 2 + piles.size() % 2);

//         for (size_t i = 0; i + 1 < piles.size(); i += 2) {
//             newPiles.push_back(mergeTwoDescendingPilesToAscending(piles[i], piles[i + 1]));
//         }
//         // Handle odd pile by merging it with the last new pile if possible
//         if (piles.size() % 2 == 1) {
//             // Reverse order of last pile to make sure it's in ascending order
//             std::reverse(piles.back().begin(), piles.back().end());
//             if (!newPiles.empty()) {
//                 newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
//             } else {
//                 newPiles.push_back(std::move(piles.back()));
//             }
//         }
//         // Update piles for the next round
//         piles = std::move(newPiles);
//     }

//     // Merge loop
//     while (piles.size() > 1) {
//         std::vector<std::vector<int>> newPiles;
//         newPiles.reserve(piles.size() / 2 + piles.size() % 2);

//         // Pairwise merge adjacent piles
//         for (size_t i = 0; i + 1 < piles.size(); i += 2) {
//             newPiles.push_back(mergeTwoAscendingPiles(piles[i], piles[i + 1]));
//         }
//         // Handle odd pile by merging it with the last new pile if possible
//         if (piles.size() % 2 == 1) {
//             if (!newPiles.empty()) {
//                 newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
//             } else {
//                 newPiles.push_back(std::move(piles.back()));
//             }
//         }
//         // Update piles for the next round
//         piles = std::move(newPiles);
//     }

//     // The final remaining pile is the sorted array
//     return piles[0];
// }

// // Iteratively merge piles until only one pile remains
// std::vector<int> mergeAscendingPiles(std::vector<std::vector<int>>& piles) {
//     // Check for early return
//     if (piles.empty()) {
//         return {};
//     }

//     // Merge loop
//     while (piles.size() > 1) {
//         std::vector<std::vector<int>> newPiles;
//         newPiles.reserve(piles.size() / 2 + piles.size() % 2);

//         // Pairwise merge adjacent piles
//         for (size_t i = 0; i + 1 < piles.size(); i += 2) {
//             newPiles.push_back(mergeTwoAscendingPiles(piles[i], piles[i + 1]));
//         }
//         // Handle odd pile by merging it with the last new pile if possible
//         if (piles.size() % 2 == 1) {
//             if (!newPiles.empty()) {
//                 newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
//             } else {
//                 newPiles.push_back(std::move(piles.back()));
//             }
//         }
//         // Update piles for the next round
//         piles = std::move(newPiles);
//     }

//     // The final remaining pile is the sorted array
//     return piles[0];
// }

// std::vector<int> mergeAdjacentPiles(
//     std::vector<std::vector<int>>& pilesDescending,
//     std::vector<std::vector<int>>& pilesAscending) {
//     // Declare sorted halves
//     std::vector<int> halfArr1;
//     std::vector<int> halfArr2;
//     halfArr1 = mergeDescendingPilesToAscending(pilesDescending);
//     halfArr2 = mergeAscendingPiles(pilesAscending);
//     // Empty piles edge cases
//     if (halfArr1.empty() && halfArr2.empty()) {
//         return halfArr1;
//     }
//     if (halfArr1.empty()) {
//         return halfArr2;
//     }
//     if (halfArr2.empty()) {
//         return halfArr1;
//     }
//     return mergeTwoAscendingPiles(halfArr1, halfArr2);
// }

// std::vector<int> mergePilesByPowersOf2(
//                 std::vector<std::vector<int>>& pilesDescending,
//                 std::vector<std::vector<int>>& pilesAscending) {
//     // Empty piles edge case
//     if (pilesDescending.empty() && pilesAscending.empty()) {
//         return pilesDescending[0];
//     }
//     int x = 1;
//     size_t len;
//     std::vector<std::vector<int>*> mergePair;  // Holds pointers to subarrays
//     mergePair.reserve(2);
//     std::vector<std::vector<int>> newPiles;
//     newPiles.reserve(pilesDescending.size() + pilesAscending.size());

//     // Merge and reverse descending half rainbow once outside loop, regardless of size
//     for (size_t i = 0; i + 1 < pilesDescending.size(); i += 2) {
//         newPiles.push_back(mergeTwoDescendingPilesToAscending(pilesDescending[i], pilesDescending[i + 1]));
//     }
//     // Handle odd pile by merging it with the last new pile if possible
//     if (pilesDescending.size() % 2 == 1) {
//         // Reverse order of last pile to make sure it's in ascending order
//         std::reverse(pilesDescending.back().begin(), pilesDescending.back().end());
//         if (!newPiles.empty()) {
//             newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), pilesDescending.back());
//         } else {
//             newPiles.push_back(std::move(pilesDescending.back()));
//         }
//     }

//     // Empty or size 1 pilesAscending edge case
//     if (pilesAscending.size() < 2) {
//         if (pilesAscending.size() == 1) {
//             if (!newPiles.empty()) {
//                 newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), pilesAscending[0]);
//             } else {
//                 newPiles.push_back(std::move(pilesAscending[0]));
//             }
//         }
//         pilesAscending = std::move(newPiles);
//     }

//     // Merge loop
//     while (pilesAscending.size() > 1) {
//         // Skip the first time because we filled this with pilesAscending
//         if (x > 1) {
//             newPiles.clear();
//         }

//         // Merge subarrays with lengths under increasing powers of 2
//         for (size_t i = 0; i < pilesAscending.size(); ++i) {
//             len = pilesAscending[i].size();
//             if (len <= (1 << x)) {  // Subarray is under max length 2^x
//                 mergePair.push_back(&pilesAscending[i]);  // Store pointer
//                 if (mergePair.size() == 2) {
//                     newPiles.push_back(mergeTwoAscendingPiles(*mergePair[0], *mergePair[1]));
//                     mergePair.clear();  // Reset after merging
//                 }
//             } else {
//                 newPiles.push_back(std::move(pilesAscending[i]));  // Move to avoid copies
//             }
//         }

//         // Handle leftover subarray
//         if (mergePair.size() == 1) {
//             if (!newPiles.empty()) {
//                 newPiles.back() = mergeTwoAscendingPiles(*mergePair[0], newPiles.back());
//             } else {
//                 newPiles.push_back(std::move(*mergePair[0]));  // Only one subarray left
//             }
//         }
//         mergePair.clear();

//         // Make sure we have enough memory allocated for pilesAscending to hold extra subarrays
//         if (newPiles.size() > pilesAscending.size()) {
//             std::vector<std::vector<int>> pilesAscending;
//             pilesAscending.reserve(newPiles.size());
//         }
//         pilesAscending = std::move(newPiles);  // Update list
//         x++;  // Move to next power of 2
//     }

//     // Return final sorted band
//     return pilesAscending[0];
// }

// // Timsort-inspired best 2 of 3 merge policy
// std::vector<int> timsortMerge(
//     std::vector<std::vector<int>>& pilesDescending,
//     std::vector<std::vector<int>>& pilesAscending) {

//     // Empty piles edge case
//     if (pilesDescending.empty() && pilesAscending.empty()) {
//         return pilesDescending[0];
//     }

//     // Make room for pilesDescending
//     pilesAscending.reserve(pilesDescending.size());
//     std::vector<std::vector<int>> stack;

//     // Merge and reverse descending half rainbow once outside loop, regardless of size
//     for (size_t i = 0; i + 1 < pilesDescending.size(); i += 2) {
//         pilesAscending.push_back(mergeTwoDescendingPilesToAscending(pilesDescending[i], pilesDescending[i + 1]));
//     }
//     // Handle odd pile by merging it with the last new pile if possible
//     if (pilesDescending.size() % 2 == 1) {
//         // Reverse order of last pile to make sure it's in ascending order
//         std::reverse(pilesDescending.back().begin(), pilesDescending.back().end());
//         if (!pilesAscending.empty()) {
//             pilesAscending.back() = mergeTwoAscendingPiles(pilesAscending.back(), pilesDescending.back());
//         } else {
//             pilesAscending.push_back(std::move(pilesDescending.back()));
//         }
//     }

//     // Empty or size 1 pilesAscending edge case
//     if (pilesAscending.empty()) {
//         return {};
//     }
//     if (pilesAscending.size() == 1) {
//         return pilesAscending[0];
//     }

//     for (auto& run : pilesAscending) {
//     // for (auto it = pilesAscending.rbegin(); it != pilesAscending.rend(); ++it) {
//     //     auto& run = *it;
//         stack.push_back(std::move(run));

//         // Greedily select best 2 out of 3 to merge
//         while (stack.size() >= 3) {
//             auto& X = stack[stack.size() - 3];
//             auto& Y = stack[stack.size() - 2];
//             auto& Z = stack[stack.size() - 1];

//             if (X.size() <= Y.size() + Z.size()) {
//                 // Merge X and Y in-place
//                 Y = mergeTwoAscendingPiles(X, Y);
//                 stack.erase(stack.end() - 3);  // Remove X
//             } else {
//                 break;
//             }
//         }
//     }

//     // Final merging phase
//     while (stack.size() > 1) {
//         stack[stack.size() - 2] = mergeTwoAscendingPiles(
//             stack[stack.size() - 2],
//             stack[stack.size() - 1]
//         );
//         stack.pop_back();  // Remove last merged element
//     }

//     return stack.empty() ? std::vector<int>{} : std::move(stack[0]);  // Move final sorted array
// }

// // Powers of 2 and best 2 of 3 merge policy, after merging descending bands once to reverse them
// std::vector<int> powersAndBestMerge(
//     std::vector<std::vector<int>>& pilesDescending,
//     std::vector<std::vector<int>>& pilesAscending) {
    
//     // Empty piles edge case
//     if (pilesDescending.empty() && pilesAscending.empty()) {
//         return pilesDescending[0];
//     }

//     // Make room for pilesDescending
//     pilesAscending.reserve(pilesDescending.size());
//     std::vector<std::vector<int>> stack;

//     // Merge and reverse descending half rainbow once outside loop, regardless of size
//     for (size_t i = 0; i + 1 < pilesDescending.size(); i += 2) {
//         pilesAscending.push_back(mergeTwoDescendingPilesToAscending(pilesDescending[i], pilesDescending[i + 1]));
//     }
//     // Handle odd pile by merging it with the last new pile if possible
//     if (pilesDescending.size() % 2 == 1) {
//         // Reverse order of last pile to make sure it's in ascending order
//         std::reverse(pilesDescending.back().begin(), pilesDescending.back().end());
//         if (!pilesAscending.empty()) {
//             pilesAscending.back() = mergeTwoAscendingPiles(pilesAscending.back(), pilesDescending.back());
//         } else {
//             pilesAscending.push_back(std::move(pilesDescending.back()));
//         }
//     }

//     // Empty or size 1 pilesAscending edge case
//     if (pilesAscending.empty()) {
//         return {};
//     }
//     if (pilesAscending.size() == 1) {
//         return pilesAscending[0];
//     }

//     // Initialize power of 2 variables
//     int x = 1;
//     size_t len;
//     std::vector<std::vector<int>> newPiles;
//     newPiles.reserve(pilesAscending.size());

//     // Merge loop
//     while (pilesAscending.size() > 1) {
//         // Skip the first time because we filled this with pilesAscending
//         if (x > 1) {
//             newPiles.clear();
//         }

//         // Merge subarrays with lengths under increasing powers of 2
//         for (size_t i = 0; i < pilesAscending.size(); ++i) {
//             len = pilesAscending[i].size();
//             if (len <= (1 << x)) {  // Subarray is under max length 2^x
//                 stack.push_back(std::move(pilesAscending[i]));

//                 // Greedily select best 2 out of 3 to merge
//                 while (stack.size() >= 3) {
//                     auto& X = stack[stack.size() - 3];
//                     auto& Y = stack[stack.size() - 2];
//                     auto& Z = stack[stack.size() - 1];

//                     if (X.size() <= Y.size() + Z.size()) {
//                         // Merge X and Y in-place
//                         Y = mergeTwoAscendingPiles(X, Y);
//                         stack.erase(stack.end() - 3);  // Remove X
//                     } else {
//                         break;
//                     }
//                 }
//             } else {
//                 newPiles.push_back(std::move(pilesAscending[i]));  // Move to avoid copies
//             }
//         }

//         // Merge leftover in the stack
//         if (!stack.empty()) {
//             while (stack.size() > 1) {
//                 stack[stack.size() - 2] = mergeTwoAscendingPiles(
//                     stack[stack.size() - 2], 
//                     stack[stack.size() - 1]
//                 );
//                 stack.pop_back();  // Remove last merged element
//             }
//             // Now merge in the one remaining stack band
//             newPiles.push_back(std::move(stack[0]));
//             stack.clear();
//         }

//         pilesAscending = std::move(newPiles);  // Update list
//         x++;  // Move to next power of 2
//     }

//     // Return final sorted band
//     return pilesAscending[0];
// }

// int findDescendingPile(std::vector<std::vector<int>>& piles, int& mid, int value) {
//     // To process natural runs in O(1), we check the current index and one adjacent
//     // band prior to the binary search loop
//     if (piles[mid].back() >= value){
//         if (mid == 0 || piles[mid-1].back() < value){
//             return mid;
//         } else {
//             if (mid-1 == 0 || piles[mid-2].back() < value){
//                 return mid-1;
//             }
//         }
//     }

//     // Binary search
//     int low = 0, high = piles.size();
//     while (low < high) {
//         mid = low + ((high - low) >> 1);
//         if (piles[mid].back() < value)
//             high = mid;
//         else
//             low = mid + 1;
//     }
//     return low;
// }

// int findAscendingPile(std::vector<std::vector<int>>& piles, int& mid, int value) {
//     // To process natural runs in O(1), we check the current index and one adjacent
//     // band prior to the binary search loop
//     if (piles[mid].back() <= value){
//         if (mid == 0 || piles[mid-1].back() > value){
//             return mid;
//         } else {
//             if (mid-1 == 0 || piles[mid-2].back() > value){
//                 return mid-1;
//             }
//         }
//     }

//     // Binary search
//     int low = 0, high = piles.size();
//     while (low < high) {
//         mid = low + ((high - low) >> 1);
//         if (piles[mid].back() > value)
//             low = mid + 1;
//         else
//             high = mid;
//     }
//     return low;
// }

int findDescendingPileWithBaseArray(std::vector<int>& baseArray, int& mid, int value) {
    int low = 0;
    int high = baseArray.size();
    if (baseArray[mid] >= value){
        if (mid == 0 || baseArray[mid-1] < value){
            return mid;
        } else {
            if (mid-1 == 0 || baseArray[mid-2] < value){
                return mid-1;
            }
        }
        high = mid;
    } else {
        low = mid + 1;
    }

    // Binary search
    while (low < high) {
        mid = low + ((high - low) >> 1);
        if (baseArray[mid] < value)
            low = mid + 1;
        else
            high = mid;
    }
    mid = low;
    return mid;
}

int findAscendingPileWithBaseArray(std::vector<int>& baseArray, int& mid, int value) {
    // Pile arrays are ascending:
    //   [1, 2, 3, 4]
    // Base array is descending:
    //   [4, 3, 2, 1]
    // We want to find where:
    //   baseArray[mid - 1] > value >= baseArray[mid]

    // To process natural runs in O(1), we check the current index and one adjacent
    // band prior to the binary search loop
    int low = 0;
    int high = baseArray.size();
    if (baseArray[mid] <= value){
        if (mid == 0 || baseArray[mid-1] > value){
            return mid;
        } else {
            if (mid-1 == 0 || baseArray[mid-2] > value){
                return mid-1;
            }
        }
        high = mid;
    } else {
        low = mid + 1;
    }

    // Binary search
    while (low < high) {
        mid = low + ((high - low) >> 1);
        if (baseArray[mid] > value)
            low = mid + 1;
        else
            high = mid;
    }
    mid = low;
    return mid;
}

// void insertValueDescendingSimulated(
//     std::vector<int>& baseArray,
//     std::vector<int>& pileSizes,
//     int& startIndex,
//     int value,
//     int& outPile,
//     int& outIndex
// ) {
//     int pileIndex = 0;
//     if (!baseArray.empty()) {
//         pileIndex = findDescendingPileWithBaseArray(baseArray, startIndex, value);
//     }

//     if (pileIndex < (int)baseArray.size()) {
//         baseArray[pileIndex] = value;
//     } else {
//         baseArray.push_back(value);
//         pileSizes.push_back(0);
//     }

//     outPile  = pileIndex;
//     outIndex = pileSizes[pileIndex]++;

//     startIndex = pileIndex;
// }

// void insertValueAscendingSimulated(
//     std::vector<int>& baseArray,
//     std::vector<int>& pileSizes,
//     int& startIndex,
//     int value,
//     int& outPile,
//     int& outIndex
// ) {
//     int pileIndex = 0;
//     if (!baseArray.empty()) {
//         pileIndex = findAscendingPileWithBaseArray(baseArray, startIndex, value);
//     }

//     if (pileIndex < (int)baseArray.size()) {
//         baseArray[pileIndex] = value;
//     } else {
//         baseArray.push_back(value);
//         pileSizes.push_back(0);
//     }

//     outPile  = pileIndex;
//     outIndex = pileSizes[pileIndex]++;

//     startIndex = pileIndex;
// }

void insertValueDescendingPiles(std::vector<std::vector<int>>& piles, std::vector<int>& baseArray, int& startIndex, int value) {
    // Descending piles are ordered by ascending tail values:
    //   [
    //     [9, 8, ... 1],
    //     [9, 8, ... 2],
    //     ...
    //     [9, 8, ... 7],
    //     [8] <- A new high value would start a pile at the back
    //   ]
    // The lowest tail values should be at the lowest indices.
    // Base array is therefore ascending:
    //   [1, 2 ... 7, 8]
    int pileIndex = 0;
    if (baseArray.size() > 0){
        //pileIndex = findDescendingPile(piles, startIndex, value);
        pileIndex = findDescendingPileWithBaseArray(baseArray, startIndex, value);
    }
    if (pileIndex < (int)baseArray.size()) {
        piles[pileIndex].push_back(value);  // Add to the appropriate pile
        baseArray[pileIndex] = value;  // Update the base array
        startIndex = pileIndex;
    } else {
        piles.emplace_back();
        piles.back().reserve(32);
        piles.back().push_back(value);  // Create a new pile
        baseArray.push_back(value); // Update base array
        startIndex = (int)piles.size() - 1;
    }
}

void insertValueAscendingPiles(std::vector<std::vector<int>>& piles, std::vector<int>& baseArray, int& startIndex, int value) {
    // Ascending piles are ordered by descending tail values:
    //   [
    //     [1, 2, ... 9],
    //     [1, 2, ... 8],
    //     ...
    //     [1, 2, ... 3],
    //     [2] <- A new low value would start a pile at the back
    //   ]
    // The highest tail values should be at the lowest indices.
    // Base array is therefore descending:
    //   [9, 8 ... 3, 2]
    int pileIndex = 0;
    if (baseArray.size() > 0){
        //pileIndex = findAscendingPile(piles, startIndex, value);
        pileIndex = findAscendingPileWithBaseArray(baseArray, startIndex, value);
    }
    if (pileIndex < (int)baseArray.size()) {
        piles[pileIndex].push_back(value);  // Add to the appropriate pile
        baseArray[pileIndex] = value;  // Update the base array
        startIndex = pileIndex;
    } else {
        piles.emplace_back();
        piles.back().reserve(32);
        piles.back().push_back(value);  // Create a new pile
        baseArray.push_back(value); // Update base array
        startIndex = (int)piles.size() - 1;
    }
}


////////////////////////////////////////////////
// JesseSort
// 1. Play 2 games of Patience, 1 with ascending stacks and 1 with descending stacks.
//    Send values to optimal game based on natural run order.
// 2. Merge stacks/bands until 1 remains.
////////////////////////////////////////////////
std::vector<int> jesseSort(std::vector<int>& arr) {

    // Initialize both half rainbows
    std::vector<std::vector<int>> pilesDescending; // Ordered by ascending tail values
    pilesDescending.reserve(2);
    //reserve_structure(pilesDescending, arr.size());
    std::vector<std::vector<int>> pilesAscending; // Ordered by descending tail values
    pilesAscending.reserve(2);
    //reserve_structure(pilesAscending, arr.size());

    // Initialize base array copies, used for faster search
    std::vector<int> pilesDescendingBaseArray; // Holds just the ascending tail values
    std::vector<int> pilesAscendingBaseArray; // Holds just the descending tail values

    int lastValueProcessed = 0;
    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;
    bool descendingMode = false;

    ////////////////////////////////////////////////
    // Phase 1: Insertion
    ////////////////////////////////////////////////

    // // Play Patience, send values to optimal game based on natural run order
    // for (int value : arr) {
    //     // NOTE: This if-statement encourages higher values to go to insertValueAscendingPiles
    //     //       and lower values to go to insertValueDescendingPiles, similar to a split rainbow.
    //     //       There may be a more optimal way to check for natural runs without influencing
    //     //       the ranges of these 2 Patience games.
    //     if (value > lastValueProcessed) {
    //         descendingMode = false;
    //     } else if (value < lastValueProcessed) {
    //         descendingMode = true;
    //     }
    //     // else this is a repeated value, so use the same descendingMode as last loop to process this one in O(1)

    //     // Insert value
    //     if (descendingMode) {
    //         insertValueDescendingPiles(pilesDescending, pilesDescendingBaseArray, lastPileIndexDescending, value);
    //     } else {
    //         insertValueAscendingPiles(pilesAscending, pilesAscendingBaseArray, lastPileIndexAscending, value);
    //     }

    //     lastValueProcessed = value;
    // }




    int i = 0;
    int n_minus_1 = arr.size() - 1;
    int x = 8;
    int n_minus_x = arr.size() - x; // We potentially do a sort8_branchless so stop early
    size_t entropy_counter = 0;
    // size_t entropy_threshold = 100;
    size_t entropy_threshold = arr.size() / (x * 100); // should scale with n (and x?)

    while (i < n_minus_x){
        // Probability of 5 ascending/descending values in a row with random input is:
        //   (1 asc + 1 desc)/(5!) = 2/120 = 0.01667 = 1.7%
        // So even though we later sort more than 5 values (e.g. sort8_branchless),
        // only 5 comparisons are actually needed to decide if the input is random
        int a0 = (arr[i] <= arr[i+1]);
        int a1 = (arr[i+1] <= arr[i+2]);
        int a2 = (arr[i+2] <= arr[i+3]);
        int a3 = (arr[i+3] <= arr[i+4]);
        bool asc = (a0 + a1 + a2 + a3) == 4;
        // int d0 = (arr[i] >= arr[i+1]);
        // int d1 = (arr[i+1] >= arr[i+2]);
        // int d2 = (arr[i+2] >= arr[i+3]);
        // int d3 = (arr[i+3] >= arr[i+4]);
        // bool desc = (d0 + d1 + d2 + d3) == 4;
        bool desc = (a0 + a1 + a2 + a3) == 0; // Technically skips repeats in descending runs



        // NOTE: Removing the if block below causes Sorted input to slow down even though it's always false/skipped
        //       and adding a std::cout line to this block causes Reverse input to speed up. Some interesting code
        //       layout sensitivity happening behind the scenes that needs to be worked out/optimized.
        // Check for random first, already fast on asc/desc
        if (!(asc | desc)) {
            asc = true;

            if (entropy_counter < entropy_threshold) {
                // Increment each time we reach this block
                entropy_counter++;

                // Optimization testing below
                //sort4_branchless(arr[i], arr[i+1], arr[i+2], arr[i+3]);
                sort8_branchless(arr[i], arr[i+1], arr[i+2], arr[i+3], arr[i+4], arr[i+5], arr[i+6], arr[i+7]);
                //sort16_branchless(&arr[i]);
                //sort32_branchless(&arr[i]);
                //std::sort(arr.begin(), arr.end());
                //std::sort(&arr[i], &arr[i+x]);
                //std::sort(arr.begin() + i, arr.end());
                //sort4_branchless(arr[i+28], arr[i+29], arr[i+30], arr[i+31]);
                //sort8_branchless(arr[i+24], arr[i+25], arr[i+26], arr[i+27], arr[i+28], arr[i+29], arr[i+30], arr[i+31]);
                //insertion_sort_32(arr, i);
                //insertion_sort_32s(&arr[i]);
                //return arr;
            } else {
                // If entropy threshold is met, input is too random so just introsort the rest
                std::sort(arr.begin() + i, arr.end());

                // Merge adjacent piles (flat concat)
                std::vector<size_t> run_starts, run_lengths;
                vectorsToFlatArrMerge(arr, pilesDescending, pilesAscending, run_starts, run_lengths);
                run_starts.push_back(i);
                run_lengths.push_back(arr.size() - i);
                return bottomUpMerge(arr, run_starts, run_lengths);
                //return bottomUpMergeWithTemp(arr, run_starts, run_lengths);
                //return best2of3(arr, run_starts, run_lengths);

                //std::sort(arr.begin(), arr.end());
                //return arr;
            }
        }



        // Check ascending or descending
        if (asc == true) {
            //while (arr[i] <= arr[i+1]){ // out of bounds
            while (i < n_minus_1 && arr[i] <= arr[i+1]) {
                insertValueAscendingPiles(pilesAscending, pilesAscendingBaseArray, lastPileIndexAscending, arr[i]);
                ++i;
            }
        } else {
            //while (arr[i] >= arr[i+1]){ // out of bounds
            // while (i + 1 < arr.size()...) here caused a large code layout sensitivity slowdown
            while (i < n_minus_1 && arr[i] >= arr[i+1]) {
                insertValueDescendingPiles(pilesDescending, pilesDescendingBaseArray, lastPileIndexDescending, arr[i]);
                ++i;
            }
        }
    }

    // Finish last few values if there are any
    while (i < arr.size()){
        if (arr[i] >= arr[i-1]){
            insertValueAscendingPiles(pilesAscending, pilesAscendingBaseArray, lastPileIndexAscending, arr[i]);
            ++i;
        } else {
            insertValueDescendingPiles(pilesDescending, pilesDescendingBaseArray, lastPileIndexDescending, arr[i]);
            ++i;
        }
    }







    // // Code for simulating games to avoid physical memory chasing
    // // NOTE: This still requires memory chasing during reconstruction, so rethink reconstruction strategy
    // // Initialize base array copies, used for faster search
    // std::vector<int> pilesDescendingBaseArray; // Holds just the ascending tail values
    // std::vector<int> pilesAscendingBaseArray; // Holds just the descending tail values

    // int lastValueProcessed = 0;
    // int lastPileIndexAscending = 0;
    // int lastPileIndexDescending = 0;
    // bool ascendingMode = true;

    // struct Placement {
    //     int val;       // value in array
    //     int game;      // 0 = ascending, 1 = descending
    //     int pile;      // pile index within that game
    //     int index;     // index within pile
    // };

    // std::vector<Placement> placements;
    // placements.reserve(arr.size()); // make sure arr.size() is correct

    // // struct {
    // //     std::vector<int> game;
    // //     std::vector<int> pile;
    // //     std::vector<int> index;
    // //     std::vector<int> val;
    // // } placements;

    // // size_t n = arr.size();
    // // placements.game.reserve(n);
    // // placements.pile.reserve(n);
    // // placements.index.reserve(n);
    // // placements.val.reserve(n);

    // // Track next index-within-pile for each pile in each game
    // std::vector<int> ascendingPileSizes;
    // std::vector<int> descendingPileSizes;

    // // Phase 1: Insertion (SIMULATED)
    // for (int value : arr) {

    //     if (value > lastValueProcessed) {
    //         ascendingMode = true;
    //     } else if (value < lastValueProcessed) {
    //         ascendingMode = false;
    //     }

    //     Placement p;

    //     if (ascendingMode) {
    //         p.game = 0; // ascending
    //         insertValueAscendingSimulated(
    //             pilesAscendingBaseArray,
    //             ascendingPileSizes,
    //             lastPileIndexAscending,
    //             value,
    //             p.pile,
    //             p.index
    //         );
    //     } else {
    //         p.game = 1; // descending
    //         insertValueDescendingSimulated(
    //             pilesDescendingBaseArray,
    //             descendingPileSizes,
    //             lastPileIndexDescending,
    //             value,
    //             p.pile,
    //             p.index
    //         );
    //     }

    //     p.val = value;
    //     placements.push_back(p);
    //     lastValueProcessed = value;
    // }

    // // Phase 1: Insertion (SIMULATED)
    // for (int value : arr) {

    //     if (value > lastValueProcessed) {
    //         ascendingMode = true;
    //     } else if (value < lastValueProcessed) {
    //         ascendingMode = false;
    //     }

    //     int game;
    //     int pile;
    //     int index;

    //     if (ascendingMode) {
    //         game = 0; // ascending
    //         insertValueAscendingSimulated(
    //             pilesAscendingBaseArray,
    //             ascendingPileSizes,
    //             lastPileIndexAscending,
    //             value,
    //             pile,
    //             index
    //         );
    //     } else {
    //         game = 1; // descending
    //         insertValueDescendingSimulated(
    //             pilesDescendingBaseArray,
    //             descendingPileSizes,
    //             lastPileIndexDescending,
    //             value,
    //             pile,
    //             index
    //         );
    //     }

    //     placements.game.push_back(game);
    //     placements.pile.push_back(pile);
    //     placements.index.push_back(index);
    //     placements.val.push_back(value);

    //     lastValueProcessed = value;
    // }

    // // Tracker variables required when we replace physical placement in simulated games
    // int numAscendingPiles = ascendingPileSizes.size();
    // int numDescendingPiles = descendingPileSizes.size();
    // std::vector<int> cumsumPileIndexStartsAscending;
    // std::vector<int> cumsumPileIndexStartsDescending;
    // std::vector<int> cumsumPileIndexStarts;
    // std::vector<int> gameStartIndices;

    // // Calculate and concat cumsum pile sizes into 1 array to simplify placement
    // cumsumPileIndexStarts.reserve(numAscendingPiles + numDescendingPiles);

    // // Ascending piles
    // int totalAscending = 0;
    // for (int sz : ascendingPileSizes) {
    //     cumsumPileIndexStarts.push_back(totalAscending);
    //     totalAscending += sz;
    // }

    // // Descending piles
    // int totalDescending = 0;
    // for (int sz : descendingPileSizes) {
    //     cumsumPileIndexStarts.push_back(totalDescending);
    //     totalDescending += sz;
    // }

    // // Concat pile sizes into 1 array to simplify placement
    // std::vector<int> pileSizes;
    // pileSizes.reserve(numAscendingPiles + numDescendingPiles);

    // // Ascending piles
    // pileSizes.insert(
    //     pileSizes.end(),
    //     ascendingPileSizes.begin(),
    //     ascendingPileSizes.end()
    // );

    // // Descending piles
    // pileSizes.insert(
    //     pileSizes.end(),
    //     descendingPileSizes.begin(),
    //     descendingPileSizes.end()
    // );

    // // gameStartIndices.resize(2);
    // // gameStartIndices[0] = 0;
    // // gameStartIndices[1] = totalAscending;

    // // // Build physical piles, but flattened into one n-size array
    // // // NOTE: We overwrite arr to save n space in memory, can free base arrays/other variables now too if it will free cache and speed things up
    // // for (Placement p : placements){
    // //     arr[
    // //         gameStartIndices[p.game] 
    // //         + cumsumPileIndexStarts[
    // //             (p.game == 0) ? p.pile : (numAscendingPiles + p.pile)
    // //         ] 
    // //     + p.index
    // //     ] = p.val;
    // // }

    // // Build physical piles, flattened into one n-size array
    // for (size_t i = 0; i < placements.val.size(); ++i) {

    //     int game  = placements.game[i];
    //     int pile  = placements.pile[i];
    //     int index = placements.index[i];
    //     int value = placements.val[i];

    //     if (game == 0){
    //         arr[cumsumPileIndexStarts[pile] + index] = value;
    //     } else {
    //         arr[totalAscending + cumsumPileIndexStarts[numAscendingPiles + pile] + index] = value;
    //     }
    // }

    // // Reverse descending stacks in postprocessing (faster than offset placement?)
    // reverseDescendingPiles(arr, pileSizes, cumsumPileIndexStarts, numAscendingPiles, numDescendingPiles, totalAscending);

    // Only use this for Phase 1 timing tests, returns arr with unmerged bands
    // return arr;


    ////////////////////////////////////////////////
    // Phase 2: Merging
    ////////////////////////////////////////////////

    // Merge adjacent piles (flat concat)
    std::vector<size_t> run_starts, run_lengths;
    vectorsToFlatArrMerge(arr, pilesDescending, pilesAscending, run_starts, run_lengths);
    return bottomUpMerge(arr, run_starts, run_lengths);
    //return bottomUpMergeWithTemp(arr, run_starts, run_lengths);
    //return best2of3(arr, run_starts, run_lengths);

    // Legacy merges for non-flat vectors of vectors
    // Merge adjacent piles (vectors of vectors)
    // return mergeAdjacentPiles(pilesDescending, pilesAscending);
    // Merge piles by length based on increasing max lengths in powers of 2
    // return mergePilesByPowersOf2(pilesDescending, pilesAscending);
    // Timsort-inspired merge logic, best 2 out of 3, (X + Y) + Z vs X + (Y + Z)
    // return timsortMerge(pilesDescending, pilesAscending);
    // Powers of 2 and best 2 out of 3
    // return powersAndBestMerge(pilesDescending, pilesAscending);


    ////////////////////////////////////////////////
    // TODO
    ////////////////////////////////////////////////
    // - Look into why Reversed input is so slow. There may be a bug in the binary
    //       search logic. Speed should be very close to Sorted times, so fixing
    //       this could speed up almost every row in the testbed!
    //       - Found partial answer: code layout sensitivity. Also, global branch
    //            prediction/history tables may be influenced by prior testbed inputs
    //            (e.g. Random -> Sorted -> Reverse).
    // - Refactor less naive merge methods to work with the new flat array.
    // - Once a random input is determined, try a larger bitonic sort or insertion sort.
    //       This is leaning into Timsort-style 32-value insertion sort runs, but it
    //       would be interesting to see if it's faster to still inject these values
    //       over existing piles vs treat them as completed piles and set them aside
    //       until Phase 2 merging. With large n, 32 value piles are very small, so
    //       it is likely better to overlay them.
    // - Implement Huffman merging, smallest 2 piles iteratively. Overhead may be slower
    //       than naive methods because of cache misses vs 2 adjacent runs already in cache.
    //       As such, the best 2 of 3 adjacent may be better: (X + Y) + Z vs X + (Y + Z).
    // - Early freezing (@62%?) of piles/baseArray lengths, so values that would
    //       make new bands instead get thrown into a new unsorted array for sorting
    //       separately. This prevents many new small bands at the suboptimal ends,
    //       which may speed up the merge phase because we're not doing so many memory
    //       lookups/trying to merge many small arrays. Could stream directly to introsort/
    //       pdqsort for the remainder?
    // - Start new games from scratch when a base array reaches some length (sqrt(n)?).
    //       Similar to freezing but instead of setting anything aside, you're just
    //       building piles again, but with a smaller search space. Overhead worth it?
    // - Revisit simulated piles after the many recent and upcoming changes. Leaving
    //       the simulated game code commented out for now, but it should get cleaned
    //       up if we're not using simulations due to cache misses during reconstruction.
}
