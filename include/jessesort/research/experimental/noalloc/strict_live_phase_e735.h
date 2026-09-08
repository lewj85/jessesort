#pragma once

// E735: strict noalloc live-phase + retained E731/E729 natural-run prepass.
// This composes the already-audited strict-native prepass with the E728
// fixed-capacity strict live-phase implementation. No adaptive merge path is
// reachable from this entry point.

#include <jessesort/research/experimental/noalloc/strict_e731_e729_prepass.h>
#include <jessesort/research/experimental/noalloc/live_phase_rebased.h>

#include <functional>
#include <type_traits>
#include <vector>

namespace jessesort::experimental::strict_live_phase_e735 {

template<class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>);
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>);
    if (a.size() >= 50000 &&
        jessesort::experimental::strict_e731::tryE729PrepassStrict(a, less)) return;
    jessesort::experimental::live_phase_noalloc::sort_strict(a, less);
}

} // namespace jessesort::experimental::strict_live_phase_e735
