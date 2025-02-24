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
    size_t vectorSize = 1<<24;

    // Create a random vector of integers
    std::vector<int> arr;
    arr.reserve(vectorSize);

    ///////////////////////////////////////////////////
    // Tests 
    ///////////////////////////////////////////////////

    // Random values
    // std::cout << "Random values" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     // vs std::uniform_int_distribution
    //     arr.push_back(std::rand() % vectorSize);
    // }

    // Random values, low diversity
    // std::cout << "Random values, low diversity" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     // vs std::uniform_int_distribution
    //     arr.push_back(std::rand() % 100);
    // }

    // Ascending values
    // std::cout << "Ascending values" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(i);
    // }

    // Ascending values, 1% random noise
    // std::cout << "Ascending values, 1% random noise" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(i);
    // }
    // for (size_t i = 0; i < vectorSize/100; ++i) {
    //     arr[std::rand() % vectorSize] = std::rand() % vectorSize;
    // }

    // Ascending values, 1% random repeated values
    // std::cout << "Ascending values, 1% random repeated values" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(i);
    // }
    // for (size_t i = 0; i < vectorSize/100; ++i) {
    //     arr[std::rand() % vectorSize] = 100;
    // }

    // Descending values
    // std::cout << "Descending values" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(vectorSize - i);
    // }

    // Descending values, 1% random noise
    // std::cout << "Descending values, 1% random noise" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(vectorSize - i);
    // }
    // for (size_t i = 0; i < vectorSize/100; ++i) {
    //     arr[std::rand() % vectorSize] = std::rand() % vectorSize;
    // }

    // Descending values, 1% random repeated values
    // std::cout << "Descending values, 1% random repeated values" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(vectorSize - i);
    // }
    // for (size_t i = 0; i < vectorSize/100; ++i) {
    //     arr[std::rand() % vectorSize] = 100;
    // }

    // Repeated values
    // std::cout << "Repeated values" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     arr.push_back(100);
    // }

    // Pyramid values
    // std::cout << "Pyramid values" << std::endl;
    // for (size_t i = 0; i < vectorSize/2; ++i) {
    //     arr.push_back(i);
    // }
    // for (size_t i = vectorSize/2; i < vectorSize; ++i) {
    //     arr.push_back(vectorSize - i);
    // }

    // Sawtooth Up
    std::cout << "Sawtooth Up" << std::endl;
    for (size_t i = 0; i < vectorSize; ++i) {
        arr.push_back(i % (vectorSize/8));
    }

    // Sawtooth Up+Down
    // std::cout << "Sawtooth Up+Down" << std::endl;
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     if ( (i/(vectorSize/8)) % 2 == 0) {
    //         arr.push_back(i % (vectorSize/8));
    //     } else {
    //         arr.push_back(vectorSize/8 - (i % (vectorSize/8)));
    //     }
    // }

    // ReLU
    // std::cout << "ReLU" << std::endl;
    // for (size_t i = 0; i < vectorSize/2; ++i) {
    //     arr.push_back(0);
    // }
    // for (size_t i = 0; i < vectorSize/2; ++i) {
    //     arr.push_back(i);
    // }

    // Mixed 1
    // std::cout << "Mixed 1" << std::endl;
    // // Random values
    // for (size_t i = 0*vectorSize/5; i < 1*vectorSize/5; ++i) {
    //     arr.push_back(std::rand() % vectorSize);
    // }
    // // Random values, low diversity
    // for (size_t i = 1*vectorSize/5; i < 2*vectorSize/5; ++i) {
    //     arr.push_back(std::rand() % 100);
    // }
    // // Ascending values
    // for (size_t i = 2*vectorSize/5; i < 3*vectorSize/5; ++i) {
    //     arr.push_back(i + 100);
    // }
    // // Descending values
    // for (size_t i = 3*vectorSize/5; i < 4*vectorSize/5; ++i) {
    //     arr.push_back(vectorSize - i - 100);
    // }
    // // Repeated values
    // for (size_t i = 4*vectorSize/5; i < 5*vectorSize/5; ++i) {
    //     arr.push_back(100);
    // }
    // // 1% Random noise
    // for (size_t i = 0; i < vectorSize/100; ++i) {
    //     arr[std::rand() % vectorSize] = std::rand() % vectorSize;
    // }

    // Mixed 2
    // std::cout << "Mixed 2" << std::endl;
    // // Random values
    // for (size_t i = 0; i < vectorSize; ++i) {
    //     // vs std::uniform_int_distribution
    //     arr.push_back(std::rand() % vectorSize);
    // }
    // // Ascending values
    // for (size_t i = vectorSize/4; i < 3*vectorSize/4; ++i) {
    //     arr[i] = i + 100;
    // }

    // Parallel zipped ascending
    // std::cout << "Parallel zipped ascending" << std::endl;
    // for (size_t i = 0; i < vectorSize/2; ++i) {
    //     arr.push_back(i);
    //     arr.push_back(i + 100);
    // }

    // Parallel ascending subsequences
    // std::cout << "Parallel ascending subsequences" << std::endl;
    // for (size_t i = 0; i < vectorSize/200; ++i) {
    //     for (size_t j = 0; j < 100; ++j) {
    //         arr.push_back(j);
    //     }
    //     for (size_t j = 0; j < 100; ++j) {
    //         arr.push_back(10000 + j);
    //     }
    // }

    // Parallel descending subsequences
    // std::cout << "Parallel descending subsequences" << std::endl;
    // for (size_t i = 0; i < vectorSize/200; ++i) {
    //     for (size_t j = 0; j < 100; ++j) {
    //         arr.push_back(100 - j);
    //     }
    //     for (size_t j = 0; j < 100; ++j) {
    //         arr.push_back(10100 - j);
    //     }
    // }

    // Alternating subsequences (Sawtooth Up+Down with gaps)
    // std::cout << "Alternating subsequences (Sawtooth Up+Down with gaps)" << std::endl;
    // for (size_t i = 0; i < vectorSize/200; ++i) {
    //     for (size_t j = 0; j < 100; ++j) {
    //         arr.push_back(j);
    //     }
    //     for (size_t j = 0; j < 100; ++j) {
    //         arr.push_back(10100 - j);
    //     }
    // }

    ///////////////////////////////////////////////////

    auto arr2 = arr;

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<int> sortedArr = jesseSort(arr);
    //jesseSort(arr);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "JesseSort: " << std::chrono::duration<double>(end - start).count() << " seconds" << std::endl;

    // std::cout << "Sorted Array: ";
    // for (int x : sortedArr) std::cout << x << " ";
    // std::cout << std::endl;

    start = std::chrono::high_resolution_clock::now();
    //std::vector<int> sortedArr = std::sort(arr2);
    std::sort(arr2.begin(), arr2.end());
    end = std::chrono::high_resolution_clock::now();
    std::cout << "std::sort: " << std::chrono::duration<double>(end - start).count() << " seconds" << std::endl;

    return 0;
}
