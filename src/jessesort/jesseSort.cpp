#include "jesseSort.hpp"
#include <iostream>
#include <algorithm>
#include <vector>

// Merge two sorted std::vectors into one sorted std::vector
// NOTE: This returns an ascending pile, reversing the order
std::vector<int> mergeTwoDescendingPiles(std::vector<int>& pile1, std::vector<int>& pile2) {
    std::vector<int> merged;
    merged.reserve(pile1.size() + pile2.size());

    // NOTE: We use int instead of size_t to let i,j go to -1, this avoids additional if-statements/breaks
    int i = pile1.size() - 1, j = pile2.size() - 1;

    // Merge loop: since piles are descending, we traverse them from the end (to get ascending order)
    while (i >= 0 && j >= 0) {
        if (pile1[i] <= pile2[j]) {
            merged.push_back(pile1[i--]);
        } else {
            merged.push_back(pile2[j--]);
        }
    }

    // Add remaining elements from either pile (if there are any)
    while (i >= 0) merged.push_back(pile1[i--]);
    while (j >= 0) merged.push_back(pile2[j--]);

    return merged;
}

// Merge two sorted std::vectors into one sorted std::vector
std::vector<int> mergeTwoAscendingPiles(std::vector<int>& pile1, std::vector<int>& pile2) {
    std::vector<int> merged;
    merged.reserve(pile1.size() + pile2.size());
    size_t i = 0, j = 0;

    // Merge loop
    while (i < pile1.size() && j < pile2.size()) {
        if (pile1[i] <= pile2[j]) {
            merged.push_back(pile1[i++]);
        } else {
            merged.push_back(pile2[j++]);
        }
    }

    // Add remaining elements
    while (i < pile1.size()) merged.push_back(pile1[i++]);
    while (j < pile2.size()) merged.push_back(pile2[j++]);

    return merged;
}

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

// Iteratively merge piles until only one pile remains, reversing to ascending order
std::vector<int> mergeDescendingPilesToAscending(std::vector<std::vector<int>>& piles) {
    // Check for early return
    if (piles.empty()) {
        return {};
    }

    // Pairwise merge adjacent piles
    // NOTE: First iteration deals with reversed order
    {
        std::vector<std::vector<int>> newPiles;
        newPiles.reserve(piles.size() / 2 + piles.size() % 2);

        for (size_t i = 0; i + 1 < piles.size(); i += 2) {
            newPiles.push_back(mergeTwoDescendingPiles(piles[i], piles[i + 1]));
        }
        // Handle odd pile by merging it with the last new pile if possible
        if (piles.size() % 2 == 1) {
            // Reverse order of last pile to make sure it's in ascending order
            std::reverse(piles.back().begin(), piles.back().end());
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
            } else {
                newPiles.push_back(std::move(piles.back()));
            }
        }
        // Update piles for the next round
        piles = std::move(newPiles);
    }

    // Merge loop
    while (piles.size() > 1) {
        std::vector<std::vector<int>> newPiles;
        newPiles.reserve(piles.size() / 2 + piles.size() % 2);

        // Pairwise merge adjacent piles
        for (size_t i = 0; i + 1 < piles.size(); i += 2) {
            newPiles.push_back(mergeTwoAscendingPiles(piles[i], piles[i + 1]));
        }
        // Handle odd pile by merging it with the last new pile if possible
        if (piles.size() % 2 == 1) {
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
            } else {
                newPiles.push_back(std::move(piles.back()));
            }
        }
        // Update piles for the next round
        piles = std::move(newPiles);
    }

    // The final remaining pile is the sorted array
    return piles[0];
}

// Iteratively merge piles until only one pile remains
std::vector<int> mergeAscendingPiles(std::vector<std::vector<int>>& piles) {
    // Check for early return
    if (piles.empty()) {
        return {};
    }

    // Merge loop   
    while (piles.size() > 1) {
        std::vector<std::vector<int>> newPiles;
        newPiles.reserve(piles.size() / 2 + piles.size() % 2);

        // Pairwise merge adjacent piles
        for (size_t i = 0; i + 1 < piles.size(); i += 2) {
            newPiles.push_back(mergeTwoAscendingPiles(piles[i], piles[i + 1]));
        }
        // Handle odd pile by merging it with the last new pile if possible
        if (piles.size() % 2 == 1) {
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
            } else {
                newPiles.push_back(std::move(piles.back()));
            }
        }
        // Update piles for the next round
        piles = std::move(newPiles);
    }

    // The final remaining pile is the sorted array
    return piles[0];
}

std::vector<int> mergeAdjacentPiles(
    std::vector<std::vector<int>>& pilesDescending,
    std::vector<std::vector<int>>& pilesAscending) {
    // Declare sorted halves
    std::vector<int> halfArr1;
    std::vector<int> halfArr2;
    halfArr1 = mergeDescendingPilesToAscending(pilesDescending);
    halfArr2 = mergeAscendingPiles(pilesAscending);
    // Empty piles edge cases
    if (halfArr1.empty() && halfArr2.empty()) {
        return halfArr1;
    }
    if (halfArr1.empty()) {
        return halfArr2;
    }
    if (halfArr2.empty()) {
        return halfArr1;
    }
    return mergeTwoAscendingPiles(halfArr1, halfArr2);
}

std::vector<int> mergePilesByPowersOf2(
                std::vector<std::vector<int>>& pilesDescending,
                std::vector<std::vector<int>>& pilesAscending) {
    // Empty piles edge case
    if (pilesDescending.empty() && pilesAscending.empty()) {
        return pilesDescending[0];
    }
    int x = 1;
    size_t len;
    std::vector<std::vector<int>*> mergePair;  // Holds pointers to subarrays
    mergePair.reserve(2);
    std::vector<std::vector<int>> newPiles;
    newPiles.reserve(pilesDescending.size() + pilesAscending.size());

    // Merge and reverse descending half rainbow once outside loop, regardless of size
    for (size_t i = 0; i + 1 < pilesDescending.size(); i += 2) {
        newPiles.push_back(mergeTwoDescendingPiles(pilesDescending[i], pilesDescending[i + 1]));
    }
    // Handle odd pile by merging it with the last new pile if possible
    if (pilesDescending.size() % 2 == 1) {
        // Reverse order of last pile to make sure it's in ascending order
        std::reverse(pilesDescending.back().begin(), pilesDescending.back().end());
        if (!newPiles.empty()) {
            newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), pilesDescending.back());
        } else {
            newPiles.push_back(std::move(pilesDescending.back()));
        }
    }

    // Empty or size 1 pilesAscending edge case
    if (pilesAscending.size() < 2) {
        if (pilesAscending.size() == 1) {
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), pilesAscending[0]);
            } else {
                newPiles.push_back(std::move(pilesAscending[0]));
            }
        }
        pilesAscending = std::move(newPiles);
    }

    // Merge loop
    while (pilesAscending.size() > 1) {
        // Skip the first time because we filled this with pilesAscending
        if (x > 1) {
            newPiles.clear();
        }

        // Merge subarrays with lengths under increasing powers of 2
        for (size_t i = 0; i < pilesAscending.size(); ++i) {
            len = pilesAscending[i].size();
            if (len <= (1 << x)) {  // Subarray is under max length 2^x
                mergePair.push_back(&pilesAscending[i]);  // Store pointer
                if (mergePair.size() == 2) {
                    newPiles.push_back(mergeTwoAscendingPiles(*mergePair[0], *mergePair[1]));
                    mergePair.clear();  // Reset after merging
                }
            } else {
                newPiles.push_back(std::move(pilesAscending[i]));  // Move to avoid copies
            }
        }

        // Handle leftover subarray
        if (mergePair.size() == 1) {
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(*mergePair[0], newPiles.back());
            } else {
                newPiles.push_back(std::move(*mergePair[0]));  // Only one subarray left
            }
        }
        mergePair.clear();

        // Make sure we have enough memory allocated for pilesAscending to hold extra subarrays
        if (newPiles.size() > pilesAscending.size()) {
            std::vector<std::vector<int>> pilesAscending;
            pilesAscending.reserve(newPiles.size());
        }
        pilesAscending = std::move(newPiles);  // Update list
        x++;  // Move to next power of 2
    }

    // Return final sorted band
    return pilesAscending[0];
}

// Timsort-inspired best 2 of 3 merge policy
std::vector<int> timsortMerge(
    std::vector<std::vector<int>>& pilesDescending,
    std::vector<std::vector<int>>& pilesAscending) {

    // Empty piles edge case
    if (pilesDescending.empty() && pilesAscending.empty()) {
        return pilesDescending[0];
    }

    // Make room for pilesDescending
    pilesAscending.reserve(pilesDescending.size());
    std::vector<std::vector<int>> stack;

    // Merge and reverse descending half rainbow once outside loop, regardless of size
    for (size_t i = 0; i + 1 < pilesDescending.size(); i += 2) {
        pilesAscending.push_back(mergeTwoDescendingPiles(pilesDescending[i], pilesDescending[i + 1]));
    }
    // Handle odd pile by merging it with the last new pile if possible
    if (pilesDescending.size() % 2 == 1) {
        // Reverse order of last pile to make sure it's in ascending order
        std::reverse(pilesDescending.back().begin(), pilesDescending.back().end());
        if (!pilesAscending.empty()) {
            pilesAscending.back() = mergeTwoAscendingPiles(pilesAscending.back(), pilesDescending.back());
        } else {
            pilesAscending.push_back(std::move(pilesDescending.back()));
        }
    }

    // Empty or size 1 pilesAscending edge case
    if (pilesAscending.empty()) {
        return {};
    }
    if (pilesAscending.size() == 1) {
        return pilesAscending[0];
    }

    for (auto& run : pilesAscending) {
    // for (auto it = pilesAscending.rbegin(); it != pilesAscending.rend(); ++it) {
    //     auto& run = *it;
        stack.push_back(std::move(run));

        // Greedily select best 2 out of 3 to merge
        while (stack.size() >= 3) {
            auto& X = stack[stack.size() - 3];
            auto& Y = stack[stack.size() - 2];
            auto& Z = stack[stack.size() - 1];

            if (X.size() <= Y.size() + Z.size()) {
                // Merge X and Y in-place
                Y = mergeTwoAscendingPiles(X, Y);
                stack.erase(stack.end() - 3);  // Remove X
            } else {
                break;
            }
        }
    }

    // Final merging phase
    while (stack.size() > 1) {
        stack[stack.size() - 2] = mergeTwoAscendingPiles(
            stack[stack.size() - 2], 
            stack[stack.size() - 1]
        );
        stack.pop_back();  // Remove last merged element
    }

    return stack.empty() ? std::vector<int>{} : std::move(stack[0]);  // Move final sorted array
}

// Powers of 2 and best 2 of 3 merge policy, after merging descending bands once to reverse them
std::vector<int> powersAndBestMerge(
    std::vector<std::vector<int>>& pilesDescending,
    std::vector<std::vector<int>>& pilesAscending) {
    
    // Empty piles edge case
    if (pilesDescending.empty() && pilesAscending.empty()) {
        return pilesDescending[0];
    }

    // Make room for pilesDescending
    pilesAscending.reserve(pilesDescending.size());
    std::vector<std::vector<int>> stack;

    // Merge and reverse descending half rainbow once outside loop, regardless of size
    for (size_t i = 0; i + 1 < pilesDescending.size(); i += 2) {
        pilesAscending.push_back(mergeTwoDescendingPiles(pilesDescending[i], pilesDescending[i + 1]));
    }
    // Handle odd pile by merging it with the last new pile if possible
    if (pilesDescending.size() % 2 == 1) {
        // Reverse order of last pile to make sure it's in ascending order
        std::reverse(pilesDescending.back().begin(), pilesDescending.back().end());
        if (!pilesAscending.empty()) {
            pilesAscending.back() = mergeTwoAscendingPiles(pilesAscending.back(), pilesDescending.back());
        } else {
            pilesAscending.push_back(std::move(pilesDescending.back()));
        }
    }

    // Empty or size 1 pilesAscending edge case
    if (pilesAscending.empty()) {
        return {};
    }
    if (pilesAscending.size() == 1) {
        return pilesAscending[0];
    }

    // Initialize power of 2 variables
    int x = 1;
    size_t len;
    std::vector<std::vector<int>> newPiles;
    newPiles.reserve(pilesAscending.size());

    // Merge loop
    while (pilesAscending.size() > 1) {
        // Skip the first time because we filled this with pilesAscending
        if (x > 1) {
            newPiles.clear();
        }

        // Merge subarrays with lengths under increasing powers of 2
        for (size_t i = 0; i < pilesAscending.size(); ++i) {
            len = pilesAscending[i].size();
            if (len <= (1 << x)) {  // Subarray is under max length 2^x
                stack.push_back(std::move(pilesAscending[i]));

                // Greedily select best 2 out of 3 to merge
                while (stack.size() >= 3) {
                    auto& X = stack[stack.size() - 3];
                    auto& Y = stack[stack.size() - 2];
                    auto& Z = stack[stack.size() - 1];

                    if (X.size() <= Y.size() + Z.size()) {
                        // Merge X and Y in-place
                        Y = mergeTwoAscendingPiles(X, Y);
                        stack.erase(stack.end() - 3);  // Remove X
                    } else {
                        break;
                    }
                }
            } else {
                newPiles.push_back(std::move(pilesAscending[i]));  // Move to avoid copies
            }
        }

        // Merge leftover in the stack
        if (!stack.empty()) {
            while (stack.size() > 1) {
                stack[stack.size() - 2] = mergeTwoAscendingPiles(
                    stack[stack.size() - 2], 
                    stack[stack.size() - 1]
                );
                stack.pop_back();  // Remove last merged element
            }
            // Now merge in the one remaining stack band
            newPiles.push_back(std::move(stack[0]));
            stack.clear();
        }

        pilesAscending = std::move(newPiles);  // Update list
        x++;  // Move to next power of 2
    }

    // Return final sorted band
    return pilesAscending[0];
}

// // Helper function to find the first pile where the value can be placed
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

// // Helper function to find the first pile where the value can be placed
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

// Helper function to find the first pile where the value can be placed
int findDescendingPileWithBaseArray(std::vector<int>& baseArray, int& mid, int value) {
    // Base array is ascending:
    //   [1, 2 ... 7, 8]
    // We want to find where:
    //   baseArray[mid - 1] < value <= baseArray[mid]

    // To process natural runs in O(1), we check the current index and one adjacent
    // band prior to the binary search loop
    if (baseArray[mid] >= value){
        if (mid == 0 || baseArray[mid-1] < value){
            return mid;
        } else {
            if (mid-1 == 0 || baseArray[mid-2] < value){
                return mid-1;
            }
        }
    }

    // Binary search
    int low = 0, high = baseArray.size();
    while (low < high) {
        mid = low + ((high - low) >> 1);
        if (baseArray[mid] < value)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

// Helper function to find the first pile where the value can be placed
int findAscendingPileWithBaseArray(std::vector<int>& baseArray, int& mid, int value) {
    // Base array is descending:
    //   [9, 8 ... 3, 2]
    // We want to find where:
    //   baseArray[mid - 1] > value >= baseArray[mid]

    // To process natural runs in O(1), we check the current index and one adjacent
    // band prior to the binary search loop
    if (baseArray[mid] <= value){
        if (mid == 0 || baseArray[mid-1] > value){
            return mid;
        } else {
            if (mid-1 == 0 || baseArray[mid-2] > value){
                return mid-1;
            }
        }
    }

    // Binary search
    int low = 0, high = baseArray.size();
    while (low < high) {
        mid = low + ((high - low) >> 1);
        if (baseArray[mid] > value)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

void insertValueDescendingSimulated(
    std::vector<int>& baseArray,
    std::vector<int>& pileSizes,
    int& startIndex,
    int value,
    int& outPile,
    int& outIndex
) {
    int pileIndex = 0;
    if (!baseArray.empty()) {
        pileIndex = findDescendingPileWithBaseArray(baseArray, startIndex, value);
    }

    if (pileIndex < (int)baseArray.size()) {
        baseArray[pileIndex] = value;
    } else {
        baseArray.push_back(value);
        pileSizes.push_back(0);
    }

    outPile  = pileIndex;
    outIndex = pileSizes[pileIndex]++;

    startIndex = pileIndex;
}

void insertValueAscendingSimulated(
    std::vector<int>& baseArray,
    std::vector<int>& pileSizes,
    int& startIndex,
    int value,
    int& outPile,
    int& outIndex
) {
    int pileIndex = 0;
    if (!baseArray.empty()) {
        pileIndex = findAscendingPileWithBaseArray(baseArray, startIndex, value);
    }

    if (pileIndex < (int)baseArray.size()) {
        baseArray[pileIndex] = value;
    } else {
        baseArray.push_back(value);
        pileSizes.push_back(0);
    }

    outPile  = pileIndex;
    outIndex = pileSizes[pileIndex]++;

    startIndex = pileIndex;
}

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
    //         [1, 2, ... 9],
    //         [1, 2, ... 8],
    //         ...
    //         [1, 2, ... 3],
    //         [2] <- A new low value would start a pile at the back
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

void showPiles(std::vector<std::vector<int>> pilesDescending, std::vector<std::vector<int>> pilesAscending) {
    std::cout << "Ascending:\n";
    for (int i = 0; i < pilesAscending.size(); ++i) {
        std::cout << "Pile " << i << ": ";
        for (int x : pilesAscending[i]) std::cout << x << " ";
        std::cout << std::endl;
    }
    std::cout << "Descending:\n";
    for (int i = 0; i < pilesDescending.size(); ++i) {
        std::cout << "Pile " << i << ": ";
        for (int x : pilesDescending[i]) std::cout << x << " ";
        std::cout << std::endl;
    }
    return;
}

// JesseSort
// 1. Play 2 games of Patience, 1 with ascending stacks and 1 with descending stacks.
//    Send values to optimal game based on natural run order.
// 2. Merge stacks/bands until 1 remains.
std::vector<int> jesseSort(std::vector<int>& arr) {

    // Initialize both half rainbows
    std::vector<std::vector<int>> pilesDescending; // Ordered by ascending tail values
    pilesDescending.reserve(2);
    std::vector<std::vector<int>> pilesAscending; // Ordered by descending tail values
    pilesAscending.reserve(2);

    // Initialize base array copies, used for faster search
    std::vector<int> pilesDescendingBaseArray; // Holds just the ascending tail values
    std::vector<int> pilesAscendingBaseArray; // Holds just the descending tail values

    int lastValueProcessed = 0;
    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;
    bool ascendingMode = true;

    ////////////////////////////////////////////////
    // Phase 1: Insertion
    ////////////////////////////////////////////////

    // Play Patience, send values to optimal game based on natural run order
    for (int value : arr) {
        // NOTE: This if-statement encourages higher values to go to insertValueAscendingPiles
        //       and lower values to go to insertValueDescendingPiles, similar to a split rainbow.
        //       There may be a more optimal way to check for natural runs without influencing
        //       the ranges of these 2 Patience games.
        if (value > lastValueProcessed) {
            ascendingMode = true;
        } else if (value < lastValueProcessed) {
            ascendingMode = false;
        }
        // else this is a repeated value, so use the same ascendingMode as last loop to process this one in O(1)

        // Insert value
        if (ascendingMode) {
            insertValueAscendingPiles(pilesAscending, pilesAscendingBaseArray, lastPileIndexAscending, value);
        } else {
            insertValueDescendingPiles(pilesDescending, pilesDescendingBaseArray, lastPileIndexDescending, value);
        }

        lastValueProcessed = value;
    }

    // // New code for simulating games to avoid physical memory chasing
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

    // Merge adjacent piles
    // return mergeAdjacentPiles(pilesDescending, pilesAscending);

    // vs

    // Merge piles by length based on increasing max lengths in powers of 2
    // return mergePilesByPowersOf2(pilesDescending, pilesAscending);

    // vs

    // Timsort merge logic, best 2 out of 3, (X + Y) + Z vs X + (Y + Z)
    return timsortMerge(pilesDescending, pilesAscending);

    // vs

    // Powers of 2 and best 2 out of 3
    // return powersAndBestMerge(pilesDescending, pilesAscending);

    // TODO: Huffman smallest 2 piles iteratively

    // TODO: Early freezing (@62%?) of piles/baseArray lengths, so values that would
    //       make new bands instead get thrown into a new unsorted array for sorting
    //       separately. This prevents many new small bands at the suboptimal ends,
    //       which may speed up the merge phase because we're not doing so many memory
    //       lookups/trying to merge many small arrays.
}
