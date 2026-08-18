#include <jessesort/jessesort_noalloc_bounded-fallback_inplace-adaptive_partition-heapsort-fallback.h>
#include <vector>

template void jessesort::allocation_free_v10::sort<int, std::less<int>>(std::vector<int>&, std::less<int>);
