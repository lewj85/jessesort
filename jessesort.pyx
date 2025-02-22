from libcpp.vector cimport vector

cdef extern from "jesseSort.hpp":
    vector[int] jesseSort(vector[int]&)

def jessesort(input_list):
    cdef vector[int] input_vector
    cdef vector[int] sorted_vector
    cdef int i, n

    # Convert Python list to C++ std::vector
    for i in input_list:
        input_vector.push_back(i)

    # Call the C++ function
    sorted_vector = jesseSort(input_vector)

    # Convert std::vector back to a Python list
    n = <int> sorted_vector.size()
    return [sorted_vector[i] for i in range(n)]
