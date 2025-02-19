#include "jesseSort.hpp"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <algorithm>

int main() {
    // Set a seed for random number generation
    std::srand(std::time(nullptr));

    // Define the size of the vector you want to test
    size_t vectorSize = 2<<23;

    // Create a random vector of integers
    std::vector<int> arr;
    arr.reserve(vectorSize);
    for (size_t i = 0; i < vectorSize; ++i) {
        // Random values between 0 and vectorSize-1
        arr.push_back(std::rand() % vectorSize);
        // vs std::uniform_int_distribution
    }
    auto arr2 = arr;

    auto start = std::chrono::high_resolution_clock::now();
    //std::vector<int> sortedArr = jesseSort(arr);
    jesseSort(arr);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "JesseSort: " << std::chrono::duration<double>(end - start).count() << std::endl;

    // std::cout << "Sorted Array: ";
    // for (int x : sortedArr) std::cout << x << " ";
    // std::cout << std::endl;
    //std::cout << sortedArr.front() << std::endl;
    //std::cout << sortedArr.back() << std::endl;

    start = std::chrono::high_resolution_clock::now();
    //std::vector<int> sortedArr = std::sort(arr2);
    std::sort(arr2.begin(), arr2.end());
    end = std::chrono::high_resolution_clock::now();
    std::cout << "std::sort: " << std::chrono::duration<double>(end - start).count() << std::endl;

    return 0;
}
