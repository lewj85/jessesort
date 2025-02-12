from libc.stdlib cimport malloc, realloc, free
from libc.math cimport sqrt

# Import NumPy
#import numpy as np
#cimport numpy as np

cdef int* merge_bands(
    int* arr1,
    int n1,
    int* arr2,
    int n2
):
    """Merges two sorted bands into one sorted band using C arrays."""

    cdef int i = 0, j = 0, k = 0
    cdef int* merged

    # Allocate memory for the merged array
    merged = <int*>malloc((n1 + n2) * sizeof(int))

    if merged == NULL:
        # Error handling in case malloc fails
        raise MemoryError("Memory allocation failed for merged array")

    # Merge the arrays
    while i < n1 and j < n2:
        if arr1[i] < arr2[j]:
            merged[k] = arr1[i]
            i += 1
        else:
            merged[k] = arr2[j]
            j += 1
        k += 1

    # Copy remaining elements from arr1
    while i < n1:
        merged[k] = arr1[i]
        i += 1
        k += 1

    # Copy remaining elements from arr2
    while j < n2:
        merged[k] = arr2[j]
        j += 1
        k += 1

    return merged


cdef int* merge_rainbow(
    int** split_rainbow,
    int* band_sizes,
    int arr_size
):
    """Merges the bands (C arrays) of a split_rainbow."""

    # NOTE: We use a split_rainbow, so n is the length of base array, not half
    cdef int n = arr_size
    cdef int i, band_size

    # Reverse order of bottom bands
    cdef int j, tmp
    for i in range(n // 2):
        band_size = band_sizes[i]

        # Reverse the current band in place
        for j in range(band_size // 2):
            tmp = split_rainbow[i][j]
            split_rainbow[i][j] = split_rainbow[i][band_size - 1 - j]
            split_rainbow[i][band_size - 1 - j] = tmp

    # First loop is a mirror
    cdef int** new_split_rainbow = <int**>malloc(n // 2 * sizeof(int*))
    cdef int tmp_idx = 0
    cdef int* merged

    for i in range(n // 2):
        # Merge split_rainbow[i] and split_rainbow[n-i-1]
        new_split_rainbow[tmp_idx] = merge_bands(split_rainbow[i], band_sizes[i], split_rainbow[n - i - 1], band_sizes[n - i - 1])
        band_sizes[tmp_idx] = band_sizes[i] + band_sizes[n - i - 1]  # Update band size
        tmp_idx += 1

    if n % 2 == 1:
        # If odd number of bands, merge the middle band now
        new_split_rainbow[tmp_idx - 1] = merge_bands(new_split_rainbow[tmp_idx - 1], band_sizes[tmp_idx - 1], split_rainbow[tmp_idx], band_sizes[tmp_idx])
        band_sizes[tmp_idx - 1] = band_sizes[tmp_idx - 1] + band_sizes[tmp_idx]  # Update band size

    # Free old split_rainbow memory (subarrays first)
    #for i in range(arr_size):
    #    free(split_rainbow[i])  # Free each subarray
    #free(split_rainbow)  # Free the pointer to subarrays

    split_rainbow = new_split_rainbow
    arr_size = n // 2  # Update the new number of bands

    # Free new_split_rainbow memory (subarrays first)
    #for i in range(arr_size):
    #    free(new_split_rainbow[i])  # Free each subarray
    #free(new_split_rainbow)  # Free the pointer to subarrays

    # Merge bands
    #cdef int** new_split_rainbow
    cdef int new_size
    while arr_size > 1:
        # Allocate temporary space for merged bands
        new_split_rainbow = <int**>malloc((arr_size // 2 + arr_size % 2) * sizeof(int*))
        new_size = 0

        for i in range(0, arr_size - 1, 2):
            # Perform the merging of two bands
            new_split_rainbow[new_size] = merge_bands(split_rainbow[i], band_sizes[i], split_rainbow[i + 1], band_sizes[i + 1])
            band_sizes[new_size] = band_sizes[i] + band_sizes[i + 1]  # Update band size
            new_size += 1

        # If odd, carry over the last band
        if arr_size % 2 == 1:
            new_split_rainbow[new_size] = split_rainbow[arr_size - 1]
            band_sizes[new_size] = band_sizes[arr_size - 1]  # The size remains the same for the last band
            new_size += 1

        # Free old split_rainbow memory (subarrays first)
        #for i in range(arr_size):
        #    free(split_rainbow[i])  # Free each subarray
        #free(split_rainbow)  # Free the pointer to subarrays

        split_rainbow = new_split_rainbow
        arr_size = new_size  # Update the new number of bands
        
        # Free new_split_rainbow memory (subarrays first)
        #for i in range(arr_size):
        #    free(new_split_rainbow[i])  # Free each subarray
        #free(new_split_rainbow)  # Free the pointer to subarrays

        # NOTE: Below is not needed as we only care about speed, leaving harmless artifacts is okay
        #band_sizes = band_sizes[:arr_size]

    return split_rainbow[0]


cdef int* make_band(int val):
    """
    Create a new band (C array) of size 32, initialize it with the value "val" at index 0.
    """
    # Allocate memory for the band
    cdef int* band
    band = <int*>malloc(32 * sizeof(int))
    if band == NULL:
        raise MemoryError("Unable to allocate memory for band.")

    # Set the first value to "val"
    band[0] = val

    return band


cdef void jessesort_with_split_lists_and_value_array(
    int*** split_rainbow,
    int** band_sizes,
    int** band_capacities,
    int** base_array,
    int* arr_size,
    int* arr_capacity,
    int* mid,
    int* target
):
    """Finds location for target in base_array and updates split_rainbow."""

    cdef int left = 0
    cdef int right = arr_size[0] - 1
    cdef int prev_idx, next_idx, halflen
    cdef int i

    # Resize split_rainbow and base_array if necessary
    if arr_size[0] == arr_capacity[0]:
        arr_capacity[0] = arr_capacity[0] * 2
        split_rainbow[0] = <int**>realloc(split_rainbow[0], arr_capacity[0] * sizeof(int*))
        if not split_rainbow[0]:
            raise MemoryError("Failed to allocate memory for split_rainbow")
        base_array[0] = <int*>realloc(base_array[0], arr_capacity[0] * sizeof(int))
        if not base_array[0]:
            raise MemoryError("Failed to allocate memory for base_array")
        band_sizes[0] = <int*>realloc(band_sizes[0], arr_capacity[0] * sizeof(int))
        if not band_sizes[0]:
            raise MemoryError("Failed to allocate memory for band_sizes")
        band_capacities[0] = <int*>realloc(band_capacities[0], arr_capacity[0] * sizeof(int))
        if not band_capacities[0]:
            raise MemoryError("Failed to allocate memory for band_capacities")

    # Binary search for insertion point
    while True:

        ########################################################################################
        # Check if mid is at an end of the Rainbow, 0 or arr_size[0]-1
        # NOTE: If we don't, then prev_idx and next_idx could be out of range
        ########################################################################################
        if mid[0] == 0:
            if target[0] <= base_array[0][0]:
                # Insert at the start of split_rainbow[0]
                if band_sizes[0][0] == band_capacities[0][0]:
                    band_capacities[0][0] = band_capacities[0][0] * 2
                    split_rainbow[0][0] = <int*>realloc(split_rainbow[0][0], band_capacities[0][0] * sizeof(int))
                    if not split_rainbow[0][0]:
                        raise MemoryError("Failed to allocate memory for split_rainbow[0]")

                split_rainbow[0][0][band_sizes[0][0]] = target[0]
                band_sizes[0][0] += 1
                base_array[0][0] = target[0]
                #return split_rainbow, band_sizes, band_capacities, base_array, arr_size, arr_capacity, mid
                return
            else:
                mid[0] += 1

        # NOTE: Not elif or can get caught in infinite loop when trying to add a new element to band 2 with 2 total bands
        if mid[0] == arr_size[0] - 1:
            if target[0] >= base_array[0][arr_size[0] - 1]:
                # Insert at the end of split_rainbow[arr_size[0] - 1]
                if band_sizes[0][arr_size[0] - 1] == band_capacities[0][arr_size[0] - 1]:
                    band_capacities[0][arr_size[0] - 1] = band_capacities[0][arr_size[0] - 1] * 2
                    split_rainbow[0][arr_size[0] - 1] = <int*>realloc(split_rainbow[0][arr_size[0] - 1], band_capacities[0][arr_size[0] - 1] * sizeof(int))
                    if not split_rainbow[0][arr_size[0] - 1]:
                        raise MemoryError(f"Failed to allocate memory for split_rainbow[0][{arr_size[0] - 1}]")

                split_rainbow[0][arr_size[0] - 1][band_sizes[0][arr_size[0] - 1]] = target[0]
                band_sizes[0][arr_size[0] - 1] += 1
                base_array[0][arr_size[0] - 1] = target[0]
                #return split_rainbow, band_sizes, band_capacities, base_array, arr_size, arr_capacity, mid
                return
            else:
                mid[0] -= 1

        prev_idx = mid[0] - 1
        next_idx = mid[0] + 1
        halflen = arr_size[0] // 2

        ########################################################################################
        # Check if mid is in the middle and we need to add a new band
        ########################################################################################
        if mid[0] == halflen:
            if arr_size[0] % 2 == 0:
                # Place new band before mid
                if target[0] < base_array[0][mid[0]] and target[0] > base_array[0][prev_idx]:
                    # Shift elements right starting from mid[0]
                    for i in range(arr_size[0], mid[0], -1):
                        split_rainbow[0][i] = split_rainbow[0][i - 1]
                        base_array[0][i] = base_array[0][i - 1]
                        band_sizes[0][i] = band_sizes[0][i - 1]
                        band_capacities[0][i] = band_capacities[0][i - 1]
                    
                    # Add new band and fix values at mid[0]
                    new_band = make_band(target[0])
                    split_rainbow[0][mid[0]] = new_band
                    base_array[0][mid[0]] = target[0]
                    band_sizes[0][mid[0]] = 1
                    band_capacities[0][mid[0]] = 32
                    arr_size[0] += 1
                    #mid[0] = mid[0]
                    return
            else:
                # Complete the middle band by placing after mid
                if target[0] >= base_array[0][mid[0]] and target[0] < base_array[0][next_idx]:
                    # Shift elements right starting from next_idx
                    for i in range(arr_size[0], next_idx, -1):
                        split_rainbow[0][i] = split_rainbow[0][i - 1]
                        base_array[0][i] = base_array[0][i - 1]
                        band_sizes[0][i] = band_sizes[0][i - 1]
                        band_capacities[0][i] = band_capacities[0][i - 1]
                    
                    # Add new band and fix values at next_idx
                    new_band = make_band(target[0])
                    split_rainbow[0][next_idx] = new_band
                    base_array[0][next_idx] = target[0]
                    band_sizes[0][next_idx] = 1
                    band_capacities[0][next_idx] = 32
                    arr_size[0] += 1
                    mid[0] = next_idx
                    return

                # Complete the middle band by placing before mid
                elif target[0] < base_array[0][mid[0]] and target[0] > base_array[0][prev_idx]:
                    # Shift elements right starting from mid[0]
                    for i in range(arr_size[0], mid[0], -1):
                        split_rainbow[0][i] = split_rainbow[0][i - 1]
                        base_array[0][i] = base_array[0][i - 1]
                        band_sizes[0][i] = band_sizes[0][i - 1]
                        band_capacities[0][i] = band_capacities[0][i - 1]
                    
                    # Add new band and fix values at mid[0]
                    new_band = make_band(target[0])
                    split_rainbow[0][mid[0]] = new_band
                    base_array[0][mid[0]] = target[0]
                    band_sizes[0][mid[0]] = 1
                    band_capacities[0][mid[0]] = 32
                    arr_size[0] += 1
                    #mid[0] = mid[0]
                    return

        elif mid[0] == halflen - 1:
            if arr_size[0] % 2 == 0:
                # Add new band above mid
                if target[0] > base_array[0][mid[0]] and target[0] < base_array[0][next_idx]:
                    # Shift elements right starting from next_idx
                    for i in range(arr_size[0], next_idx, -1):
                        split_rainbow[0][i] = split_rainbow[0][i - 1]
                        base_array[0][i] = base_array[0][i - 1]
                        band_sizes[0][i] = band_sizes[0][i - 1]
                        band_capacities[0][i] = band_capacities[0][i - 1]
                    
                    # Add new band and fix values at next_idx
                    new_band = make_band(target[0])
                    split_rainbow[0][next_idx] = new_band
                    base_array[0][next_idx] = target[0]
                    band_sizes[0][next_idx] = 1
                    band_capacities[0][next_idx] = 32
                    arr_size[0] += 1
                    mid[0] = next_idx
                    return

        ########################################################################################
        # Check if we can place the target at mid location
        ########################################################################################
        # BOTTOM HALF
        if mid[0] < halflen and target[0] <= base_array[0][mid[0]] and target[0] > base_array[0][prev_idx]:
            # Resize the band if necessary
            if band_sizes[0][mid[0]] == band_capacities[0][mid[0]]:
                band_capacities[0][mid[0]] = band_capacities[0][mid[0]] * 2
                split_rainbow[0][mid[0]] = <int*>realloc(split_rainbow[0][mid[0]], band_capacities[0][mid[0]] * sizeof(int))
                if not split_rainbow[0][mid[0]]:
                    raise MemoryError(f"Failed to allocate memory for split_rainbow[0][{mid[0]}]")

            # Insert low target[0] at end of (reversed) mid[0] band
            split_rainbow[0][mid[0]][band_sizes[0][mid[0]]] = target[0]
            band_sizes[0][mid[0]] += 1  # Update band size
            base_array[0][mid[0]] = target[0]  # Replace in base_array[0]
            return

        # TOP HALF
        elif mid[0] >= halflen and target[0] >= base_array[0][mid[0]] and target[0] < base_array[0][next_idx]:
            # Resize the band if necessary
            if band_sizes[0][mid[0]] == band_capacities[0][mid[0]]:
                band_capacities[0][mid[0]] = band_capacities[0][mid[0]] * 2
                split_rainbow[0][mid[0]] = <int*>realloc(split_rainbow[0][mid[0]], band_capacities[0][mid[0]] * sizeof(int))
                if not split_rainbow[0][mid[0]]:
                    raise MemoryError(f"Failed to allocate memory for split_rainbow[0][{mid[0]}]")

            # Insert high target[0] at end of mid[0] band
            split_rainbow[0][mid[0]][band_sizes[0][mid[0]]] = target[0]
            band_sizes[0][mid[0]] += 1  # Update band size
            base_array[0][mid[0]] = target[0]  # Replace in base_array[0]
            return

        ########################################################################################
        # Adjust search bounds and update mid for next loop
        ########################################################################################
        if target[0] < base_array[0][mid[0]]:
            right = prev_idx
            if right < 0:
                right = 0
        elif target[0] > base_array[0][mid[0]]:
            left = next_idx
            if left > arr_size[0] - 1:
                left = arr_size[0] - 1

        mid[0] = (right + left) // 2


#cdef void display_split_rainbow(
#    int** split_rainbow,
#    int* band_sizes,
##    int* base_array,
#    int arr_size
#):
#    cdef int i, j
#    print("split_rainbow:")
#    for i in range(arr_size):
#        print("band", i, "with size", band_sizes[i])
#        for j in range(band_sizes[i]):
#            print(split_rainbow[i][j])
##    print("base_array:")
##    for i in range(arr_size):
##        print(base_array[i])
#    return


# NOTE: Need wrapper function to be a python "def" not "cdef"
#cdef int* jessesort(object unsorted_object):
def jessesort(object unsorted_object):
    """JesseSort wrapper function. Creates a split_rainbow and merges its bands."""
    
    cdef int* unsorted_array
    cdef int n
    cdef int i
    
    # Check if the input is a Python list or any sequence-like object
    if isinstance(unsorted_object, (list, tuple)):
        # Get the size of the list/tuple
        n = len(unsorted_object)
        # Allocate memory for the C array
        unsorted_array = <int*>malloc(n * sizeof(int))
        if unsorted_array == NULL:
            raise MemoryError("Memory allocation failed for unsorted_array")
        
        # Convert the input list to a C array
        for i in range(n):
            unsorted_array[i] = <int>unsorted_object[i]

    # Handle the case where the input is a NumPy array
    #elif isinstance(unsorted_object, np.ndarray):
    #    # Get the size of the NumPy array
    #    n = unsorted_object.shape[0]
    #    # Allocate memory for the C array
    #    unsorted_array = <int*>malloc(n * sizeof(int))
    #    if unsorted_array == NULL:
    #        raise MemoryError("Memory allocation failed for unsorted_array")
    #    
    #    # Convert the NumPy array to a C array of cdef ints
    #    for i in range(n):
    #        # Convert each element to cdef int
    #        # This ensures the conversion for various dtypes (e.g., np.int32, np.float64, etc.)
    #        unsorted_array[i] = <int>unsorted_object[i]
    
    else:
        raise TypeError("Input must be a list, tuple, or numpy array")
    
    cdef int mid = 0
    
    # Check for trivial cases
    if n < 2:
        return unsorted_object  # not unsorted_array
    
    # Compute initial allocation size for preallocation
    #cdef int init_size = <int>((sqrt(2.0 * n) / 4.0) + 2)
    cdef int init_size = 2

    # Create Rainbow
    # NOTE: We don't use lists or numpy arrays, we use C arrays
    #cdef list split_rainbow
    #cdef np.ndarray[object, ndim=1] split_rainbow = np.empty(init_size, dtype=object)
    # Declare split_rainbow as a C array of pointers (each pointing to a C subarray of ints)
    cdef int** split_rainbow = <int**>malloc(init_size * sizeof(int*))
    cdef int* band_sizes = <int*>malloc(init_size * sizeof(int))
    cdef int* band_capacities = <int*>malloc(init_size * sizeof(int))

    # Create base array
    #cdef np.ndarray[np.int_t, ndim=1] base_array = np.empty(init_size, dtype=np.int32)
    cdef int* base_array = <int*>malloc(init_size * sizeof(int))
    cdef int arr_size = 2  # Current number of elements in base_array
    cdef int arr_capacity = init_size  # Current allocated size
    
    # Initialize first two values
    val1, val2 = unsorted_array[0], unsorted_array[1]
    
    if val1 <= val2:
        split_rainbow[0] = make_band(val1)
        band_sizes[0] = 1
        band_capacities[0] = 32
        split_rainbow[1] = make_band(val2)
        band_sizes[1] = 1
        band_capacities[1] = 32
        base_array[0] = val1
        base_array[1] = val2
    else:
        split_rainbow[0] = make_band(val2)
        band_sizes[0] = 1
        band_capacities[0] = 32
        split_rainbow[1] = make_band(val1)
        band_sizes[1] = 1
        band_capacities[1] = 32
        base_array[0] = val2
        base_array[1] = val1

    # Process remaining values
    for i in range(2, n):
        #split_rainbow, band_sizes, band_capacities, base_array, arr_size, arr_capacity, mid = \
        jessesort_with_split_lists_and_value_array(
            &split_rainbow, &band_sizes, &band_capacities, &base_array, &arr_size, &arr_capacity, &mid, &(unsorted_array[i])
        )
    
    # Merge sorted bands
    sorted_result = merge_rainbow(split_rainbow, band_sizes, arr_size)

    py_list = [sorted_result[i] for i in range(band_sizes[0])]
    
    # Free allocated memory
    free(split_rainbow)
    free(base_array)
    free(sorted_result)
    
    return py_list
