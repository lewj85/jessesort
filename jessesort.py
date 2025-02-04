from typing import Any, List, Tuple, Optional


class Node:
    """A node in a linked list."""
    def __init__(self, data) -> None:
        self.data: int = data
        self.next: Optional['Node'] = None

    def add_previous(self, data) -> Any:
        """Adds a node before this node with new data."""
        N = Node(data)
        N.next = self
        return N

    def add_next(self, data) -> Any:
        """Adds a node after this node with new data."""
        self.next = Node(data)
        return self.next


def jessesort_with_nodes(
    arr_nodes: List[Node],
    mid: int,
    target: Any,
) -> Tuple[List[Node], int]:
    """
    JesseSort with linked lists for bands. Base array is still a list.

    Parameters:
    arr_nodes (list): The base array (start+end nodes of the rainbow bands).
    mid (int): Index of where we are currently in the base array.
    target (int): New value to add to the rainbow.

    Returns:
    tuple: A tuple containing multiple values:
        - arr_nodes (list): The updated base array.
        - mid (int): The updated index.

    Example:
    >>> jessesort_with_nodes([<list of nodes with 1,3,5,7,9>], 0, 0)
    [<list of nodes with 0,3,5,7,9>], 0

    Notes:
    - Random access of linked list nodes can be slow.
    """
    
    # Initialize left and right
    left, right = 0, len(arr_nodes)-1

    # Loop until return
    while True:

        # Handle the case when mid is at the first or last element
        if mid == 0:
            if target <= arr_nodes[0].data:
                new_node = arr_nodes[0].add_previous(target)
                arr_nodes[0] = new_node
                return arr_nodes, 0 # Replaced first element
            else:
                mid += 1
        if mid == len(arr_nodes)-1:
            if target >= arr_nodes[-1].data:
                new_node = arr_nodes[-1].add_next(target)
                arr_nodes[-1] = new_node
                return arr_nodes, len(arr_nodes)-1 # Replaced last element
            else:
                mid -= 1

        prev_idx = mid - 1
        next_idx = mid + 1
        halflen = len(arr_nodes)//2
    
        # Handle the case when we are in the middle and need to add a node
        if mid == halflen:
            if len(arr_nodes)%2 == 0:
                # Add node before mid
                # NOTE: Only try to add nodes before
                if target < arr_nodes[mid].data and target > arr_nodes[prev_idx].data: # >=?
                    new_node = arr_nodes[mid].add_previous(target)
                    arr_nodes.insert(mid, new_node)
                    return arr_nodes, mid
            else: # len(arr_nodes)%2 == 1:
                # Add node after mid
                # NOTE: Need >= here! Or we can't add a 2nd 3 to [1,3,5]
                if target >= arr_nodes[mid].data and target < arr_nodes[next_idx].data:
                    new_node = arr_nodes[mid].add_next(target)
                    arr_nodes.insert(next_idx, new_node)
                    return arr_nodes, next_idx
                # Add node before mid
                elif target < arr_nodes[mid].data and target > arr_nodes[prev_idx].data:
                    new_node = arr_nodes[mid].add_previous(target)
                    arr_nodes.insert(mid, new_node)
                    return arr_nodes, mid
        elif mid == halflen-1:
            if len(arr_nodes)%2 == 0:
                # Add node after mid
                # NOTE: Only try to add nodes after
                if target > arr_nodes[mid].data and target < arr_nodes[next_idx].data: # >= ?
                    new_node = arr_nodes[mid].add_next(target)
                    arr_nodes.insert(next_idx, new_node)
                    return arr_nodes, next_idx
    
        # Check if target can replace value at mid
        # Bottom half
        if mid < halflen and target <= arr_nodes[mid].data and target > arr_nodes[prev_idx].data:
            # Place the target at mid
            new_node = arr_nodes[mid].add_previous(target)
            arr_nodes[mid] = new_node
            return arr_nodes, mid
        # Top half
        # NOTE: Need >= for when len(arr_nodes)%2 == 0?
        elif mid >= halflen and target >= arr_nodes[mid].data and target < arr_nodes[next_idx].data:
            # Place the target at mid
            new_node = arr_nodes[mid].add_next(target)
            arr_nodes[mid] = new_node
            return arr_nodes, mid
    
        # If the target is less, adjust the right boundary
        if target < arr_nodes[mid].data:
            right = prev_idx
            if right < 0:
                right = 0
        # If the target is greater, adjust the left boundary
        elif target > arr_nodes[mid].data:
            left = next_idx
            if left > len(arr_nodes)-1:
                left = len(arr_nodes)-1
    
        # Update mid index and loop again
        mid = (right + left)//2


def jessesort_with_nodes_and_value_array(
    arr_nodes: List[Node],
    arr_values: List[Any],
    mid: int,
    target: Any,
) -> Tuple[List[Node], List[Any], int]:
    """
    JesseSort with linked list nodes but a 2nd base array is used for quick value lookups.

    Parameters:
    arr_nodes (list): The base array (start+end nodes of the rainbow bands).
    arr_values (list): A copy of the base array with just values.
    mid (int): Index of where we are currently in the base array.
    target (int): New value to add to the rainbow.

    Returns:
    tuple: A tuple containing multiple values:
        - arr_nodes (list): The updated base array.
        - arr_values (list): The updated value-only copy of the base array.
        - mid (int): The updated index.

    Example:
    >>> jessesort_with_nodes_and_value_array([<list of nodes with 1,3,5,7,9>], [1,3,5,7,9], 0)
    [<list of nodes with 0,3,5,7,9>], [0,3,5,7,9], 0

    Notes:
    - Random access of node values can be slow, so a value-only copy of base array is used for comparisons.
      Ideally it is small enough to fit into L1-L3 cache for faster access than accessing nodes on heap.
    """
    
    # Initialize left and right
    left, right = 0, len(arr_values)-1

    # Loop until return
    while True:

        # Handle the case when mid is at the first or last element
        if mid == 0:
            if target <= arr_values[0]:
                new_node = arr_nodes[0].add_previous(target)
                arr_nodes[0] = new_node
                arr_values[0] = target
                return arr_nodes, arr_values, 0 # Replaced first element
            else:
                mid += 1
        if mid == len(arr_values)-1:
            if target >= arr_values[-1]:
                new_node = arr_nodes[-1].add_next(target)
                arr_nodes[-1] = new_node
                arr_values[-1] = target
                return arr_nodes, arr_values, len(arr_values)-1 # Replaced last element
            else:
                mid -= 1

        prev_idx = mid - 1
        next_idx = mid + 1
        halflen = len(arr_values)//2
    
        # Handle the case when we are in the middle and need to add a node
        if mid == halflen:
            if len(arr_values)%2 == 0:
                # Add node before mid
                # NOTE: Only try to add nodes before
                if target < arr_values[mid] and target > arr_values[prev_idx]: # >=?
                    new_node = arr_nodes[mid].add_previous(target)
                    arr_nodes.insert(mid, new_node)
                    arr_values.insert(mid, target)
                    return arr_nodes, arr_values, mid
            else: # len(arr_nodes)%2 == 1:
                # Add node after mid
                # NOTE: Need >= here! Or we can't add a 2nd 3 to [1,3,5]
                if target >= arr_values[mid] and target < arr_values[next_idx]:
                    new_node = arr_nodes[mid].add_next(target)
                    arr_nodes.insert(next_idx, new_node)
                    arr_values.insert(next_idx, target)
                    return arr_nodes, arr_values, next_idx
                # Add node before mid
                elif target < arr_values[mid] and target > arr_values[prev_idx]:
                    new_node = arr_nodes[mid].add_previous(target)
                    arr_nodes.insert(mid, new_node)
                    arr_values.insert(mid, target)
                    return arr_nodes, arr_values, mid
        elif mid == halflen-1:
            if len(arr_nodes)%2 == 0:
                # Add node after mid
                # NOTE: Only try to add nodes after
                if target > arr_values[mid] and target < arr_values[next_idx]: # >= ?
                    new_node = arr_nodes[mid].add_next(target)
                    arr_nodes.insert(next_idx, new_node)
                    arr_values.insert(next_idx, target)
                    return arr_nodes, arr_values, next_idx
    
        # Check if target can replace value at mid
        # Bottom half
        if mid < halflen and target <= arr_values[mid] and target > arr_values[prev_idx]:
            # Place the target at mid
            new_node = arr_nodes[mid].add_previous(target)
            arr_nodes[mid] = new_node
            arr_values[mid] = target
            return arr_nodes, arr_values, mid
        # Top half
        # NOTE: Need >= for when len(arr)%2 == 0?
        elif mid >= halflen and target >= arr_values[mid] and target < arr_values[next_idx]:
            # Place the target at mid
            new_node = arr_nodes[mid].add_next(target)
            arr_nodes[mid] = new_node
            arr_values[mid] = target
            return arr_nodes, arr_values, mid
    
        # If the target is less, adjust the right boundary
        if target < arr_values[mid]:
            right = prev_idx
            if right < 0:
                right = 0
        # If the target is greater, adjust the left boundary
        elif target > arr_values[mid]:
            left = next_idx
            if left > len(arr_values)-1:
                left = len(arr_values)-1
    
        # Update mid index and loop again
        mid = (right + left)//2


def jessesort_with_split_lists_and_value_array(
    split_rainbow: List[List[Any]],
    arr_values: List[Any],
    mid: int,
    target: Any,
) -> Tuple[List[Node], List[Any], int]:
    """
    JesseSort with split lists for bands instead of linked lists. Lists are split by top/bottom
    half of the rainbow. Bottom half list order is reversed. A second value array is used for speed.

    Parameters:
    split_rainbow (list): The current rainbow, a list of lists. The first half of the lists are
                          in descending order, the second half of the lists are in ascending order.
    arr_values (list): A list of the end values of each subarray.
    mid (int): Index of where we are currently in the base array.
    target (int): New value to add to the rainbow.

    Returns:
    tuple: A tuple containing multiple values:
        - split_rainbow (list): The updated split rainbow.
        - arr_values (list): The updated base array.
        - mid (int): The updated index.

    Example:
    >>> jessesort_with_split_lists_and_value_array([[1],[3],[5],[7],[9]], [1,3,5,7,9], 0)
    [[1,0],[3],[5],[7],[9]], [0,3,5,7,9], 0

    Notes:
    - Random access of linked list nodes is potentially slow, so lists are used instead.
    - Inserting values at the front of lists is slow too, so lists are split and bottom half lists
      are reversed for faster appending.
    - A value-only copy of base array is used for comparisons. Ideally it is small enough to fit into
      L1-L3 cache for faster access than accessing band lists on heap.

    TODO:
    - Current implementation will make many new lists of size 1 for new values in the middle,
      even if those values are already sorted (in either direction). Find a way to add these
      to an existing list or make a new list with all these sequential values rather than build
      a giant split rainbow with many small length lists.
    - Speedup ideas:
      - First merge, can merge lists from top and bottom half without having to compare every value,
        since bottom values will be generally low and top generally high, just fix cross-section and
        can append [bottom values reversed + cross-section + top values] for each pair of bands.
      - Smarter merging policies that deal with band length spikes from natural runs (e.g. powersort)
      - For ints, use counting sort for middle elements once middle 2 values of arr_values are within a reasonable range.
    """

    # Initialize left and right
    left, right = 0, len(arr_values)-1

    # Loop until return
    while True:

        # Handle the case when mid is at the first or last element
        if mid == 0:
            if target <= arr_values[0]:
                # Replace first element
                split_rainbow[0].append(target)
                arr_values[0] = target
                return split_rainbow, arr_values, 0
            else:
                mid += 1
        if mid == len(arr_values)-1:
            if target >= arr_values[-1]:
                # Replace last element
                split_rainbow[-1].append(target)
                arr_values[-1] = target
                return split_rainbow, arr_values, len(arr_values)-1
            else:
                mid -= 1

        prev_idx = mid - 1
        next_idx = mid + 1
        halflen = len(arr_values)//2
    
        # Handle the case when we are in the middle and need to add a node
        if mid == halflen:
            if len(arr_values)%2 == 0:
                # Add node before mid
                # NOTE: Only try to add nodes before
                if target < arr_values[mid] and target > arr_values[prev_idx]: # >=?
                    split_rainbow.insert(mid, [target])
                    arr_values.insert(mid, target)
                    return split_rainbow, arr_values, mid
            else: # len(arr_nodes)%2 == 1:
                # Add node after mid
                # NOTE: Need >= here! Or we can't add a 2nd 3 to [1,3,5]
                if target >= arr_values[mid] and target < arr_values[next_idx]:
                    split_rainbow.insert(next_idx, [target])
                    arr_values.insert(next_idx, target)
                    return split_rainbow, arr_values, next_idx
                # Add node before mid
                elif target < arr_values[mid] and target > arr_values[prev_idx]:
                    split_rainbow.insert(mid, [target])
                    arr_values.insert(mid, target)
                    return split_rainbow, arr_values, mid
        elif mid == halflen-1:
            if len(arr_values)%2 == 0:
                # Add node after mid
                # NOTE: Only try to add nodes after
                if target > arr_values[mid] and target < arr_values[next_idx]: # >= ?
                    split_rainbow.insert(next_idx, [target])
                    arr_values.insert(next_idx, target)
                    return split_rainbow, arr_values, next_idx
    
        # Check if target can replace value at mid
        # NOTE: These can be combined with a large "or" because code blocks are the same.
        #       Keeping separate for clarity, but can improve this logic for speed tests!
        # Bottom half
        if mid < halflen and target <= arr_values[mid] and target > arr_values[prev_idx]:
            # Place the target at mid
            split_rainbow[mid].append(target)
            arr_values[mid] = target
            return split_rainbow, arr_values, mid
        # Top half
        # NOTE: Need >= for when len(arr)%2 == 0?
        elif mid >= halflen and target >= arr_values[mid] and target < arr_values[next_idx]:
            # Place the target at mid
            split_rainbow[mid].append(target)
            arr_values[mid] = target
            return split_rainbow, arr_values, mid
    
        # If the target is less, adjust the right boundary
        if target < arr_values[mid]:
            right = prev_idx
            if right < 0:
                right = 0
        # If the target is greater, adjust the left boundary
        elif target > arr_values[mid]:
            left = next_idx
            if left > len(arr_values)-1:
                left = len(arr_values)-1
    
        # Update mid index and loop again
        mid = (right + left)//2


def merge_linked_lists(
    node1: Any,
    node2: Any,
) -> List[Any]:
    """
    Merges 2 linked lists of nodes.

    Parameters:
    node1 (Node): First node of a sorted linked list.
    node2 (Node): First node of a second sorted linked list.

    Returns:
    merged (list): The two lists merged into one sorted list of values (not nodes).

    Example:
    >>> merge_linked_lists([<list of nodes with 1,3,5>], [<list of nodes with 2,4,6>])
    [1,2,3,4,5,6]

    Notes:
    - This returns a list of values, not a list of nodes. Nodes are no longer needed
      during the merge process and will slow things down.
    """
    merged = []

    # Merge elements from both arrays while both have elements
    while node1 and node2:
        if node1.data < node2.data:
            merged.append(node1.data)
            node1 = node1.next
        else:
            merged.append(node2.data)
            node2 = node2.next

    # If there are any remaining elements in arr1, add them to merged
    while node1:
        merged.append(node1.data)
        node1 = node1.next

    # If there are any remaining elements in arr2, add them to merged
    while node2:
        merged.append(node2.data)
        node2 = node2.next

    return merged


def merge_lists(
    arr1: List[Any],
    arr2: List[Any],
) -> List[Any]:
    """
    Merges 2 lists.

    Parameters:
    arr1 (list): First sorted list.
    arr2 (list): Second sorted list.

    Returns:
    merged (list): The two lists merged into one sorted list.

    Example:
    >>> merge_lists([1,3,5], [2,4,6])
    [1,2,3,4,5,6]
    """
    merged = []
    i = j = 0

    # Merge elements from both arrays while both have elements
    while i < len(arr1) and j < len(arr2):
        if arr1[i] < arr2[j]:
            merged.append(arr1[i])
            i += 1
        else:
            merged.append(arr2[j])
            j += 1

    # If there are any remaining elements in arr1, add them to merged
    while i < len(arr1):
        merged.append(arr1[i])
        i += 1

    # If there are any remaining elements in arr2, add them to merged
    while j < len(arr2):
        merged.append(arr2[j])
        j += 1

    return merged


def merge_rainbow(
    rainbow: List[Any],
    method: str = "lists",
    merge_policy: str = "mirror",
) -> List[Any]:
    """
    Converts rainbow into a fully sorted list by merging bands recursively.

    Parameters:
    rainbow (list): The rainbow with sorted bands.
    method (str): One of "lists", "nodes", or "nodes and values". Default is "list".
    merge_policy (str): One of "adjacent", "mirror", or "powersort". Default is "mirror".

    Returns:
    rainbow[0] (list): The final remaining band containing all sorted values after all merges.

    Example:
    >>> merge_rainbow(<rainbow with 2 bands [1,2,6] and [3,4,5]>)
    [1,2,3,4,5,6]

    Notes:
    - First merge logic can be improved as the pairs of bands in "mirror" mode will have
      significantly fewer overlapping values. This allows a "bottom + merged middle + top"
      approach that avoids the need for comparisons in the bottom and top sections. Consider
      a binary search strategy for identifying the start index of "merged middle".
    """

    # If using nodes, first do one merge pass that converts the nodes to lists
    if method == "nodes" or method == "nodes and values":

        # Need to account for odd numbers of bands as well as check for a middle band
        # NOTE: The rainbow here is really just the base array with nodes, so it includes
        #       the end nodes of each band too.
        tmp_list = []
        if (len(rainbow)//2)%2 == 0:
            for i in range(0, len(rainbow)//2, 2):
                tmp_list.append(merge_linked_lists(rainbow[i], rainbow[i+1]))
            if len(rainbow)%2 == 1:
                # Middle band has 1 value, just merge it now
                tmp_list[-1] = merge_lists(tmp_list[-1], [rainbow[len(rainbow)//2].data])
        else: # if (len(rainbow)//2)%2 == 1:
            for i in range(0, len(rainbow)//2-1, 2):
                    tmp_list.append(merge_linked_lists(rainbow[i], rainbow[i+1]))
            if len(rainbow)%2 == 0:
                tmp_list.append(merge_linked_lists(rainbow[len(rainbow)//2-1], []))
            else: # if len(rainbow)%2 == 1:
                # Middle band has 1 value, just merge it now
                tmp_list.append(merge_linked_lists(rainbow[len(rainbow)//2-1], rainbow[len(rainbow)//2]))
        rainbow = tmp_list[:]

    # Else if using lists, first reverse the bottom half lists
    elif method == "lists":
        
        # Reverse order of bottom bands
        # NOTE: Implementing actual merge logic here for the first merge that handles these reversed
        #       bottom half arrays may be faster.
        for i in range(len(rainbow)//2):
            rainbow[i] = rainbow[i][::-1]

    # Merge adjacent bands - not so naive as adjacent bands are likely to have similar lengths
    if merge_policy == "adjacent":

        # Full merge loop
        while len(rainbow) > 1:
            tmp_list = []
            # NOTE: -1 below prevents out-of-bounds but will sometimes kick the bucket down the road
            #       instead of merging the final band each iteration. This may result in a small last
            #       band occasionally having to merge with a larger one, which is not ideal for speed.
            for i in range(0, len(rainbow)-1, 2):
                tmp_list.append(merge_lists(rainbow[i], rainbow[i+1]))
            if len(rainbow)%2 == 1:
                tmp_list.append(rainbow[-1])
            rainbow = tmp_list[:]

        return rainbow[0]
    
    # Merge bands from opposite ends - ie. rainbow[0] & rainbow[-1], rainbow[1] & rainbow[-2], etc
    elif merge_policy == "mirror":

        # First mirror loop deals with reversed order of bottom bands
        tmp_list = []
        for i in range(len(rainbow)//2):
            #tmp_list.append(merge_lists(rainbow[i][::-1], rainbow[-i-1]))
            tmp_list.append(merge_lists(rainbow[i], rainbow[-i-1]))
        if len(rainbow)%2 == 1:
            # NOTE: Rather than append the remaining middle list (of length 1), just merge it now
            #tmp_list.append(rainbow[len(rainbow)//2])
            tmp_list[-1] = merge_lists(tmp_list[-1], rainbow[len(rainbow)//2])
        rainbow = tmp_list[:]

        # Full merge loop
        while len(rainbow) > 1:
            tmp_list = []
            # NOTE: -1 below prevents out-of-bounds but will sometimes kick the bucket down the road
            #       instead of merging the final band each iteration. This may result in a small last
            #       band occasionally having to merge with a larger one, which is not ideal for speed.
            for i in range(0, len(rainbow)-1, 2):
                tmp_list.append(merge_lists(rainbow[i], rainbow[i+1]))
            if len(rainbow)%2 == 1:
                tmp_list.append(rainbow[-1])
            rainbow = tmp_list[:]

        return rainbow[0]

    # Merge bands using adaptive powersort methods
    # NOTE: The overhead here may not be worth it in many cases as mirror will naturally pair
    #       bands of similar length in non-natural-run cases.
    elif merge_policy == "powersort":
        pass


def jessesort(
    unsorted_array: List[Any],
    method: str = "lists",
    merge_policy: str = "mirror",
) -> List[Any]:
    """
    Wrapper for various JesseSort functions. Builds a rainbow and merges its bands.

    Parameters:
    unsorted_array (list): The original unsorted list.
    method (str): One of "lists", "nodes", or "nodes and values". Default is "lists".
    merge_policy (str): One of "adjacent", "mirror", or "powersort". Default is "mirror".

    Returns:
    sorted_array (list): The final sorted array.

    Example:
    >>> jessesort([2,6,3,1,4,5])
    [1,2,3,4,5,6]
    """

    if method == "lists":

        # Manually add the first 2 values to create the split rainbow
        val1 = unsorted_array[0]
        val2 = unsorted_array[1]
        if val1 <= val2:
            split_rainbow = [[val1], [val2]]
            arr_values = [val1, val2]
        else:
            split_rainbow = [[val2], [val1]]
            arr_values = [val2, val1]

        # Loop through the rest of the values
        # NOTE: Using "for target in unsorted_array[2:]" will make an unnecessary copy,
        #       so use direct indexing instead of slicing
        mid = 0
        for i in range(2, len(unsorted_array)):
            split_rainbow, arr_values, mid = jessesort_with_split_lists_and_value_array(
                split_rainbow, arr_values, mid, unsorted_array[i])

        # Merge the split rainbow
        return merge_rainbow(split_rainbow, method=method, merge_policy=merge_policy)

    elif method == "nodes":

        # Manually add the first 2 values to create the split rainbow
        val1 = unsorted_array[0]
        val2 = unsorted_array[1]
        if val1 <= val2:
            arr_nodes = [Node(val1), Node(val2)]
            arr_values = [val1, val2]
        else:
            arr_nodes = [Node(val2), Node(val1)]
            arr_values = [val2, val1]

        # Loop through the rest of the values
        # NOTE: Using "for target in unsorted_array[2:]" will make an unnecessary copy,
        #       so use direct indexing instead of slicing
        mid = 0
        for i in range(2, len(unsorted_array)):
            arr_nodes, mid = jessesort_with_nodes(
                arr_nodes, mid, unsorted_array[i])
        
        # Merge the split rainbow
        return merge_rainbow(arr_nodes, method=method, merge_policy=merge_policy)

    elif method == "nodes and values":
        # Manually add the first 2 values to create the split rainbow
        val1 = unsorted_array[0]
        val2 = unsorted_array[1]
        if val1 <= val2:
            arr_nodes = [Node(val1), Node(val2)]
            arr_values = [val1, val2]
        else:
            arr_nodes = [Node(val2), Node(val1)]
            arr_values = [val2, val1]

        # Loop through the rest of the values
        # NOTE: Using "for target in unsorted_array[2:]" will make an unnecessary copy,
        #       so use direct indexing instead of slicing
        mid = 0
        for i in range(2, len(unsorted_array)):
            arr_nodes, arr_values, mid = jessesort_with_nodes_and_value_array(
                arr_nodes, arr_values, mid, unsorted_array[i])
        
        # Merge the split rainbow
        return merge_rainbow(arr_nodes, method=method, merge_policy=merge_policy)
