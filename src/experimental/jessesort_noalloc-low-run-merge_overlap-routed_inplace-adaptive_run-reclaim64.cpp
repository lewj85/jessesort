#include <jessesort/experimental/jessesort_noalloc-low-run-merge_overlap-routed_inplace-adaptive_run-reclaim64.h>
#include <vector>

template void jessesort::allocation_free_low_run::sort<int, std::less<int>>(std::vector<int>&, std::less<int>);
