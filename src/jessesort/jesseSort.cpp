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

// Helper function to find the first pile where the value can be placed
int findDescendingPile(std::vector<std::vector<int>>& piles, int& mid, int value) {
    int low = 0, high = piles.size() - 1;
    while (low <= high) {
        if (piles[mid].back() >= value) {
            // This relies on C++ short-circuit evaluation to prevent out-of-bounds error
            if (mid == 0 || piles[mid - 1].back() < value) {
                return mid;
            }
            high = mid - 1;
        } else {
            low = mid + 1;
        }
        mid = low + (high - low) / 2;
    }
    return low;
}

// Helper function to find the first pile where the value can be placed
int findAscendingPile(std::vector<std::vector<int>>& piles, int& mid, int value) {
    int low = 0, high = piles.size() - 1;
    while (low <= high) {
        if (piles[mid].back() <= value) {
            // This relies on C++ short-circuit evaluation to prevent out-of-bounds error
            if (mid == 0 || piles[mid - 1].back() > value) {
                return mid;
            }
            high = mid - 1;
        } else {
            low = mid + 1;
        }
        mid = low + (high - low) / 2;
    }
    return low;
}

void insertValueDescendingPiles(std::vector<std::vector<int>>& piles, int& startIndex, int value) {
    int pileIndex = findDescendingPile(piles, startIndex, value);
    if (pileIndex < piles.size()) {
        piles[pileIndex].push_back(value);  // Add to the appropriate pile
    } else {
        piles.emplace_back().reserve(32);
        piles.back().push_back(value);  // Create a new pile
    }
}

void insertValueAscendingPiles(std::vector<std::vector<int>>& piles, int& startIndex, int value) {
    int pileIndex = findAscendingPile(piles, startIndex, value);
    if (pileIndex < piles.size()) {
        piles[pileIndex].push_back(value);  // Add to the appropriate pile
    } else {
        piles.emplace_back().reserve(32);
        piles.back().push_back(value);  // Create a new pile
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
    std::vector<std::vector<int>> pilesDescending;
    pilesDescending.reserve(2);
    std::vector<std::vector<int>> pilesAscending;
    pilesAscending.reserve(2);
    std::vector<std::vector<int>>* whichHalfRainbow = &pilesAscending;
    int lastValueProcessed = 0;
    // TODO: Track last indices to potentially insert in O(n) time
    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;
    int* whichIndex = &lastPileIndexAscending;
    // Declare a function pointer for which sort
    void (*whichPatienceSort)(std::vector<std::vector<int>>&, int&, int) = insertValueAscendingPiles;

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
            whichPatienceSort = &insertValueAscendingPiles;
            whichHalfRainbow = &pilesAscending;
            whichIndex = &lastPileIndexAscending;
        } else if (value < lastValueProcessed) {
            whichPatienceSort = &insertValueDescendingPiles;
            whichHalfRainbow = &pilesDescending;
            whichIndex = &lastPileIndexDescending;
        }
        // else this is a repeated value, so use the same half rainbow as last loop to process this one in O(n)

        // Insert value
        whichPatienceSort(*whichHalfRainbow, *whichIndex, value);
        lastValueProcessed = value;
    }

    ////////////////////////////////////////////////
    // Phase 2: Merging
    ////////////////////////////////////////////////

    // Merge adjacent piles
    // return mergeAdjacentPiles(pilesDescending, pilesAscending);

    // vs

    // Merge piles by length based on increasing max lengths in powers of 2
    return mergePilesByPowersOf2(pilesDescending, pilesAscending);

    // vs

    // TODO: Huffman smallest 2 piles iteratively
}
