#include "patienceSort.hpp"
//#include <iostream>
#include <algorithm>
#include <vector>

// Merge two sorted std::vectors into one sorted std::vector
// NOTE: This returns an ascending pile, reversing the order
std::vector<int> mergeTwoDescendingPiles(std::vector<int>& pile1, std::vector<int>& pile2) {
    std::vector<int> merged;
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
    // Pairwise merge adjacent piles
    // NOTE: First iteration deals with reversed order
    std::vector<std::vector<int>> newPiles;
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
        newPiles.push_back(piles.back());
        }
    }
    // Update piles for the next round
    piles = newPiles;
    
    while (piles.size() > 1) {
        std::vector<std::vector<int>> newPiles;
        // Pairwise merge adjacent piles
        for (size_t i = 0; i + 1 < piles.size(); i += 2) {
            newPiles.push_back(mergeTwoAscendingPiles(piles[i], piles[i + 1]));
        }
        // Handle odd pile by merging it with the last new pile if possible
        if (piles.size() % 2 == 1) {
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
            } else {
                newPiles.push_back(piles.back());
            }
        }
        // Update piles for the next round
        piles = newPiles;
    }
    // The final remaining pile is the sorted array
    return piles[0];
}

// Iteratively merge piles until only one pile remains
std::vector<int> mergeAscendingPiles(std::vector<std::vector<int>>& piles) {
    // Pairwise merge adjacent piles    
    while (piles.size() > 1) {
        std::vector<std::vector<int>> newPiles;
        // Pairwise merge adjacent piles
        for (size_t i = 0; i + 1 < piles.size(); i += 2) {
            newPiles.push_back(mergeTwoAscendingPiles(piles[i], piles[i + 1]));
        }
        // Handle odd pile by merging it with the last new pile if possible
        if (piles.size() % 2 == 1) {
            if (!newPiles.empty()) {
                newPiles.back() = mergeTwoAscendingPiles(newPiles.back(), piles.back());
            } else {
                newPiles.push_back(piles.back());
            }
        }
        // Update piles for the next round
        piles = newPiles;
    }
    // The final remaining pile is the sorted array
    return piles[0];
}

// Helper function to find the first pile where the value can be placed
int findDescendingPile(std::vector<std::vector<int>>& piles, int value) {
    int low = 0, high = piles.size() - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (piles[mid].back() >= value) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return low;
}

// Helper function to find the first pile where the value can be placed
int findAscendingPile(std::vector<std::vector<int>>& piles, int value) {
    int low = 0, high = piles.size() - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (piles[mid].back() <= value) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return low;
}

std::vector<std::vector<int>> createDescendingPiles(std::vector<int>& arr) {
    std::vector<std::vector<int>> piles;
    for (int value : arr) {
        int pileIndex = findDescendingPile(piles, value);
        if (pileIndex < piles.size()) {
            piles[pileIndex].push_back(value);  // Add to the appropriate pile
        } else {
            piles.push_back({value});  // Create a new pile
        }

        // // Debug: Print piles after every insertion
        // std::cout << "Piles after adding " << value << ":\n";
        // for (int i = 0; i < piles.size(); ++i) {
        //     std::cout << "Pile " << i << ": ";
        //     for (int x : piles[i]) std::cout << x << " ";
        //     std::cout << std::endl;
        // }
    }
    return piles;
}

std::vector<std::vector<int>> createAscendingPiles(std::vector<int>& arr) {
    std::vector<std::vector<int>> piles;
    for (int value : arr) {
        int pileIndex = findAscendingPile(piles, value);
        if (pileIndex < piles.size()) {
            piles[pileIndex].push_back(value);  // Add to the appropriate pile
        } else {
            piles.push_back({value});  // Create a new pile
        }

        // // Debug: Print piles after every insertion
        // std::cout << "Piles after adding " << value << ":\n";
        // for (int i = 0; i < piles.size(); ++i) {
        //     std::cout << "Pile " << i << ": ";
        //     for (int x : piles[i]) std::cout << x << " ";
        //     std::cout << std::endl;
        // }
    }
    return piles;
}

// Patience sort implementation with descending piles (original)
std::vector<int> patienceSortDescendingPiles(std::vector<int>& arr) {

    // Step 1: Create piles
    std::vector<std::vector<int>> piles;
    piles = createDescendingPiles(arr);

    // Step 2: Merge piles into sorted array
    std::vector<int> sortedArr;
    sortedArr = mergeDescendingPilesToAscending(piles);

    return sortedArr;
}

// Patience sort implementation with ascending piles
std::vector<int> patienceSortAscendingPiles(std::vector<int>& arr) {

    // Step 1: Create piles
    std::vector<std::vector<int>> piles;
    piles = createAscendingPiles(arr);

    // Step 2: Merge piles into sorted array
    std::vector<int> sortedArr;
    sortedArr = mergeAscendingPiles(piles);

    return sortedArr;
}
