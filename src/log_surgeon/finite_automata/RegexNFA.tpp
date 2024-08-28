#ifndef LOG_SURGEON_FINITE_AUTOMATA_REGEX_NFA_TPP
#define LOG_SURGEON_FINITE_AUTOMATA_REGEX_NFA_TPP

#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <stack>
#include <utility>

namespace log_surgeon::finite_automata {
template <RegexNFAStateType state_type>
void RegexNFAState<state_type>::add_interval(Interval interval, RegexNFAState* dest_state) {
    if (interval.first < cSizeOfByte) {
        uint32_t const bound = std::min(interval.second, cSizeOfByte - 1);
        for (uint32_t i = interval.first; i <= bound; i++) {
            add_byte_transition(i, dest_state);
        }
        interval.first = bound + 1;
    }
    if constexpr (RegexNFAStateType::UTF8 == state_type) {
        if (interval.second < cSizeOfByte) {
            return;
        }
        std::unique_ptr<std::vector<typename Tree::Data>> overlaps
                = m_tree_transitions.pop(interval);
        for (typename Tree::Data const& data : *overlaps) {
            uint32_t overlap_low = std::max(data.m_interval.first, interval.first);
            uint32_t overlap_high = std::min(data.m_interval.second, interval.second);

            std::vector<RegexNFAUTF8State*> tree_states = data.m_value;
            tree_states.push_back(dest_state);
            m_tree_transitions.insert(Interval(overlap_low, overlap_high), tree_states);
            if (data.m_interval.first < interval.first) {
                m_tree_transitions.insert(
                        Interval(data.m_interval.first, interval.first - 1),
                        data.m_value
                );
            } else if (data.m_interval.first > interval.first) {
                m_tree_transitions.insert(
                        Interval(interval.first, data.m_interval.first - 1),
                        {dest_state}
                );
            }
            if (data.m_interval.second > interval.second) {
                m_tree_transitions.insert(
                        Interval(interval.second + 1, data.m_interval.second),
                        data.m_value
                );
            }
            interval.first = data.m_interval.second + 1;
        }
        if (interval.first != 0 && interval.first <= interval.second) {
            m_tree_transitions.insert(interval, {dest_state});
        }
    }
}

template <typename NFAStateType>
void RegexNFA<NFAStateType>::reverse() {
    // add new end with all accepting pointing to it
    NFAStateType* new_end = new_state();
    for (std::unique_ptr<NFAStateType>& state_ptr : m_states) {
        if (state_ptr->is_accepting()) {
            state_ptr->add_epsilon_transition(new_end);
            state_ptr->set_accepting(false);
        }
    }
    // move edges from NFA to maps
    std::map<std::pair<NFAStateType*, NFAStateType*>, std::vector<uint8_t>> byte_edges;
    std::map<std::pair<NFAStateType*, NFAStateType*>, bool> epsilon_edges;
    for (std::unique_ptr<NFAStateType>& src_state_ptr : m_states) {
        // TODO: handle utf8 case with if constexpr (RegexNFAUTF8State ==
        // NFAStateType) ~ don't really need this though
        for (uint32_t byte = 0; byte < cSizeOfByte; byte++) {
            for (NFAStateType* dest_state_ptr : src_state_ptr->get_byte_transitions(byte)) {
                std::pair<NFAStateType*, NFAStateType*> edge{src_state_ptr.get(), dest_state_ptr};
                byte_edges[edge].push_back(byte);
            }
            src_state_ptr->clear_byte_transitions(byte);
        }
        for (NFAStateType* dest_state_ptr : src_state_ptr->get_epsilon_transitions()) {
            epsilon_edges
                    [std::pair<NFAStateType*, NFAStateType*>(src_state_ptr.get(), dest_state_ptr)]
                    = true;
        }
        src_state_ptr->clear_epsilon_transitions();
    }

    // insert edges from maps back into NFA, but in the reverse direction
    for (std::unique_ptr<NFAStateType>& src_state_ptr : m_states) {
        for (std::unique_ptr<NFAStateType>& dest_state_ptr : m_states) {
            std::pair<NFAStateType*, NFAStateType*> key(src_state_ptr.get(), dest_state_ptr.get());
            auto byte_it = byte_edges.find(key);
            if (byte_it != byte_edges.end()) {
                for (uint8_t byte : byte_it->second) {
                    dest_state_ptr->add_byte_transition(byte, src_state_ptr.get());
                }
            }
            auto epsilon_it = epsilon_edges.find(key);
            if (epsilon_it != epsilon_edges.end()) {
                dest_state_ptr->add_epsilon_transition(src_state_ptr.get());
            }
        }
    }

    // propagate tag from old accepting m_states
    for (NFAStateType* old_accepting_state : new_end->get_epsilon_transitions()) {
        int tag = old_accepting_state->get_tag();
        std::stack<NFAStateType*> unvisited_states;
        std::set<NFAStateType*> visited_states;
        unvisited_states.push(old_accepting_state);
        while (!unvisited_states.empty()) {
            NFAStateType* current_state = unvisited_states.top();
            current_state->set_tag(tag);
            unvisited_states.pop();
            visited_states.insert(current_state);
            for (uint32_t byte = 0; byte < cSizeOfByte; byte++) {
                std::vector<NFAStateType*> byte_transitions
                        = current_state->get_byte_transitions(byte);
                for (NFAStateType* next_state : byte_transitions) {
                    if (false == visited_states.contains(next_state)) {
                        unvisited_states.push(next_state);
                    }
                }
            }
            for (NFAStateType* next_state : current_state->get_epsilon_transitions()) {
                if (false == visited_states.contains(next_state)) {
                    unvisited_states.push(next_state);
                }
            }
        }
    }
    for (int32_t i = m_states.size() - 1; i >= 0; --i) {
        std::unique_ptr<NFAStateType>& src_state_unique_ptr = m_states[i];
        NFAStateType* src_state = src_state_unique_ptr.get();
        int tag = src_state->get_tag();
        for (uint32_t byte = 0; byte < cSizeOfByte; byte++) {
            std::vector<NFAStateType*> byte_transitions = src_state->get_byte_transitions(byte);
            for (int32_t j = byte_transitions.size() - 1; j >= 0; --j) {
                NFAStateType*& dest_state = byte_transitions[j];
                if (dest_state == m_root) {
                    dest_state = new_state();
                    assert(dest_state != nullptr);
                    dest_state->set_tag(tag);
                    dest_state->set_accepting(true);
                }
            }
            src_state->clear_byte_transitions(byte);
            src_state->set_byte_transitions(byte, byte_transitions);
        }
        std::vector<NFAStateType*> epsilon_transitions = src_state->get_epsilon_transitions();
        for (int32_t j = epsilon_transitions.size() - 1; j >= 0; --j) {
            NFAStateType*& dest_state = epsilon_transitions[j];
            if (dest_state == m_root) {
                dest_state = new_state();
                dest_state->set_tag(src_state->get_tag());
                dest_state->set_accepting(true);
            }
        }
        src_state->clear_epsilon_transitions();
        src_state->set_epsilon_transitions(epsilon_transitions);
    }

    for (uint32_t i = 0; i < m_states.size(); i++) {
        if (m_states[i].get() == m_root) {
            m_states.erase(m_states.begin() + i);
            break;
        }
    }
    // start from the end
    m_root = new_end;
}

template <typename NFAStateType>
auto RegexNFA<NFAStateType>::new_state() -> NFAStateType* {
    std::unique_ptr<NFAStateType> ptr = std::make_unique<NFAStateType>();
    NFAStateType* state = ptr.get();
    m_states.push_back(std::move(ptr));
    return state;
}
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_REGEX_NFA_TPP
