#include "patienceSort.hpp"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

int main() {
    // Set a seed for random number generation
    std::srand(std::time(nullptr));

    // Define the size of the vector you want to test
    size_t vectorSize = 100;

    // Create a random vector of integers
    std::vector<int> arr(vectorSize);
    for (size_t i = 0; i < vectorSize; ++i) {
        // Random values between 0 and vectorSize-1
        arr[i] = std::rand() % vectorSize;
        // vs std::uniform_int_distribution
    }

    std::vector<int> sortedArr = patienceSort(arr);

    std::cout << "Sorted Array: ";
    for (int x : sortedArr) std::cout << x << " ";
    std::cout << std::endl;

    return 0;
}
