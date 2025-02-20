#include "jesseSort.hpp"
#include <iostream>
#include <algorithm>
#include <vector>

// Merge two sorted std::vectors into one sorted std::vector
// NOTE: This returns an ascending pile, reversing the order
std::vector<int> mergeTwoDescendingPiles(std::vector<int>& pile1, std::vector<int>& pile2) {
    std::vector<int> merged;
    merged.reserve(pile1.size() + pile2.size());

    // NOTE: We use int instead of size_t to avoid additional if-statements/breaks that push i,j negative
    int i = pile1.size() - 1, j = pile2.size() - 1;

    // Merge step: since piles are descending, we traverse them from the end (to get ascending order)
    while (i >= 0 && j >= 0) {
        if (pile1[i] <= pile2[j]) {
            merged.push_back(pile1[i]);
            i--;
        } else {
            merged.push_back(pile2[j]);
            j--;
        }
    }

    // Add remaining elements from either pile (if there are any)
    while (i >= 0) {
        merged.push_back(pile1[i]);
        i--;
    }
    while (j >= 0) {
        merged.push_back(pile2[j]);
        j--;
    }

    return merged;
}

// Merge two sorted std::vectors into one sorted std::vector
std::vector<int> mergeTwoAscendingPiles(std::vector<int>& pile1, std::vector<int>& pile2) {
    std::vector<int> merged;
    merged.reserve(pile1.size() + pile2.size());
    size_t i = 0, j = 0;

    // Merge step
    while (i < pile1.size() && j < pile2.size()) {
        if (pile1[i] <= pile2[j]) {
            merged.push_back(pile1[i]);
            i++;
        } else {
            merged.push_back(pile2[j]);
            j++;
        }
    }

    // Add remaining elements
    while (i < pile1.size()) {
        merged.push_back(pile1[i]);
        i++;
    }
    while (j < pile2.size()) {
        merged.push_back(pile2[j]);
        j++;
    }

    return merged;
}

// Iteratively merge piles until only one pile remains, reversing to ascending order
std::vector<int> mergeDescendingPilesToAscending(std::vector<std::vector<int>>& piles) {
    // Check for early return
    if (piles.empty()) {
        std::vector<int> emptyVector;
        return emptyVector;
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
        std::vector<int> emptyVector;
        return emptyVector;
    }

    // Pairwise merge adjacent piles    
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

// Helper function to find the first pile where the value can be placed
int findDescendingPile(std::vector<std::vector<int>>& piles, int& mid, int value) {
    int low = 0, high = piles.size() - 1;
    while (low <= high) {
        if (piles[mid].back() >= value) {
            // This relies on C++ short-circuit evaluation to prevent out-of-bounds error
            // if (mid == 0 || piles[mid - 1].back() > value) {
            //     return mid;
            // }
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
            // if (mid == 0 || piles[mid - 1].back() < value) {
            //     return mid;
            // }
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

// JesseSort
// 1. Play 2 games of Patience, 1 w/ ascending stacks, 1 w/ descending stacks.
//    Send values to optimal game based on natural run order.
// 2. Merge results using Powersort's near-optimal merge tree.
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


    // Play Patience, send values to optimal game based on natural run order
    for (int value : arr) {

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

        // // Debug: Print piles after every insertion
        // std::cout << "Piles after adding " << value << ":\n";
        // std::cout << "Ascending:\n";
        // for (int i = 0; i < pilesAscending.size(); ++i) {
        //     std::cout << "Pile " << i << ": ";
        //     for (int x : pilesAscending[i]) std::cout << x << " ";
        //     std::cout << std::endl;
        // }
        // std::cout << "Descending:\n";
        // for (int i = 0; i < pilesDescending.size(); ++i) {
        //     std::cout << "Pile " << i << ": ";
        //     for (int x : pilesDescending[i]) std::cout << x << " ";
        //     std::cout << std::endl;
        // }
    }

    // Merge piles into sorted array
    // TODO: Include and use Powersort merge tree code https://github.com/sebawild/powersort
    std::vector<int> halfArr1;
    std::vector<int> halfArr2;
    std::vector<int> sortedArr;
    halfArr1 = mergeDescendingPilesToAscending(pilesDescending);
    halfArr2 = mergeAscendingPiles(pilesAscending);

    // Empty piles edge cases
    if (halfArr1.empty() && halfArr2.empty()) {
        return sortedArr;
    }
    if (halfArr1.empty()) {
        return halfArr2;
    }
    if (halfArr2.empty()) {
        return halfArr1;
    }

    // Merge results from both games
    sortedArr = mergeTwoAscendingPiles(halfArr1, halfArr2);
    
    return sortedArr;
}
