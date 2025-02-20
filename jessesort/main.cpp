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
    size_t vectorSize = 2<<24;

    // Create a random vector of integers
    std::vector<int> arr;
    arr.reserve(vectorSize);

    ///////////////////////////////////////////////////
    // Tests 
    ///////////////////////////////////////////////////

    // Random values
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     // vs std::uniform_int_distribution
    //     arr.push_back(std::rand() % vectorSize);
    // }

    // Random values, low diversity
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     // vs std::uniform_int_distribution
    //     arr.push_back(std::rand() % 100);
    // }

    // Ascending
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(i);
    // }

    // Ascending, random noise
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(i);
    // }
    // for (size_t i = 0; i < 1000; ++i) {
    //     arr[std::rand() % vectorSize] = 0;
    // }

    // Descending
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(vectorSize - i);
    // }

    // Descending, random noise
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(vectorSize - i);
    // }
    // for (size_t i = 0; i < 1000; ++i) {
    //     arr[std::rand() % vectorSize] = 0;
    // }

    // Pyramid
    for (size_t i = 0; i < vectorSize/2; ++i) {
        arr.push_back(i);
    }
    for (size_t i = vectorSize/2; i < vectorSize; ++i) {
        arr.push_back(vectorSize - i);
    }

    // Sawtooth Up
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(i % 1000);
    // }

    // Sawtooth Up+Down
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     if ( (i/1000) % 2 == 0) {
    //         arr.push_back(i % 1000);
    //     } else {
    //         arr.push_back(1000 - (i % 1000));
    //     }
    // }

    // ReLU
    // for (size_t i = 0; i < vectorSize/2; ++i) {
    //     arr.push_back(0);
    // }
    // for (size_t i = vectorSize/2; i < vectorSize; ++i) {
    //     arr.push_back(i);
    // }

    // Mixed
    // for (size_t i = 0; i < vectorSize/3; ++i) {
    //     arr.push_back(i);
    // }
    // for (size_t i = vectorSize/3; i < 2*vectorSize/3; ++i) {
    //     arr.push_back(std::rand() % vectorSize);
    // }
    // for (size_t i = 2*vectorSize/3; i < vectorSize; ++i) {
    //     arr.push_back(0);
    // }

    ///////////////////////////////////////////////////

    auto arr2 = arr;

    auto start = std::chrono::high_resolution_clock::now();
    //std::vector<int> sortedArr = jesseSort(arr);
    jesseSort(arr);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "JesseSort: " << std::chrono::duration<double>(end - start).count() << std::endl;

    // std::cout << "Sorted Array: ";
    // for (int x : sortedArr) std::cout << x << " ";
    // std::cout << std::endl;

    start = std::chrono::high_resolution_clock::now();
    //std::vector<int> sortedArr = std::sort(arr2);
    std::sort(arr2.begin(), arr2.end());
    end = std::chrono::high_resolution_clock::now();
    std::cout << "std::sort: " << std::chrono::duration<double>(end - start).count() << std::endl;

    return 0;
}
