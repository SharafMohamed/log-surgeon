#ifndef LOG_SURGEON_FINITE_AUTOMATA_DFA_STATE_PAIR
#define LOG_SURGEON_FINITE_AUTOMATA_DFA_STATE_PAIR

#include <set>
#include <vector>

#include <log_surgeon/finite_automata/DfaState.hpp>

namespace log_surgeon::finite_automata {
/**
 * Class for a pair of DFA states, where each state in the pair belongs to a different DFA.
 * This class is used to facilitate the construction of an intersection DFA from two separate DFAs.
 * Each instance represents a state in the intersection DFA and follows these rules:
 *
 * - A pair is considered accepting if both states are accepting in their respective DFAs.
 * - A pair is considered reachable if both its states are reachable in their respective DFAs
 *   from this pair's states.
 *
 * NOTE: Only the first state in the pair contains the variable types matched by the pair.
 */
template <typename DFAState>
class DfaStatePair {
public:
    DfaStatePair(DFAState const* state1, DFAState const* state2)
            : m_state1(state1),
              m_state2(state2) {};

    /**
     * Used for ordering in a set by considering the states' addresses.
     * @param rhs
     * @return Whether `m_state1` in lhs has a lower address than in rhs, or if they're equal,
     * whether `m_state2` in lhs has a lower address than in rhs.
     */
    auto operator<(DfaStatePair const& rhs) const -> bool {
        if (m_state1 == rhs.m_state1) {
            return m_state2 < rhs.m_state2;
        }
        return m_state1 < rhs.m_state1;
    }

    /**
     * Generates all pairs reachable from the current pair via any string and store any reachable
     * pair not previously visited in `unvisited_pairs`.
     * @param visited_pairs Previously visited pairs.
     * @param unvisited_pairs Set to add unvisited reachable pairs.
     */
    auto get_reachable_pairs(
            std::set<DfaStatePair>& visited_pairs,
            std::set<DfaStatePair>& unvisited_pairs
    ) const -> void;

    [[nodiscard]] auto is_accepting() const -> bool {
        return m_state1->is_accepting() && m_state2->is_accepting();
    }

    [[nodiscard]] auto get_matching_variable_ids() const -> std::vector<uint32_t> const& {
        return m_state1->get_matching_variable_ids();
    }

private:
    DFAState const* m_state1;
    DFAState const* m_state2;
};

template <typename DFAState>
auto DfaStatePair<DFAState>::get_reachable_pairs(
        std::set<DfaStatePair>& visited_pairs,
        std::set<DfaStatePair>& unvisited_pairs
) const -> void {
    // TODO: Handle UTF-8 (multi-byte transitions) as well
    for (uint32_t i = 0; i < cSizeOfByte; i++) {
        auto next_state1 = m_state1->next(i);
        auto next_state2 = m_state2->next(i);
        if (next_state1 != nullptr && next_state2 != nullptr) {
            DfaStatePair reachable_pair{next_state1, next_state2};
            if (visited_pairs.count(reachable_pair) == 0) {
                unvisited_pairs.insert(reachable_pair);
            }
        }
    }
}
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_DFA_STATE_PAIR
