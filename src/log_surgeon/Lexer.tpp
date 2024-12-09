#ifndef LOG_SURGEON_LEXER_TPP
#define LOG_SURGEON_LEXER_TPP

#include <cassert>
#include <stack>
#include <string>
#include <vector>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>

/**
 * utf8 format (https://en.wikipedia.org/wiki/UTF-8)
 * 1 byte: 0x0 - 0x80 : 0xxxxxxx
 * 2 byte: 0x80 - 0x7FF : 110xxxxx 10xxxxxx
 * 3 byte: 0x800 - 0xFFFF : 1110xxxx 10xxxxxx 10xxxxxx
 * 4 byte: 0x10000 - 0x1FFFFF : 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */
namespace log_surgeon {
template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::flip_states(uint32_t old_storage_size) -> void {
    if (m_match_pos >= old_storage_size / 2) {
        m_match_pos -= old_storage_size / 2;
    } else {
        m_match_pos += old_storage_size / 2;
    }
    // TODO when m_start_pos == old_storage_size / 2, theres two possible cases
    // currently so both options are potentially wrong
    if (m_start_pos > old_storage_size / 2) {
        m_start_pos -= old_storage_size / 2;
    } else {
        m_start_pos += old_storage_size / 2;
    }
    if (m_last_match_pos >= old_storage_size / 2) {
        m_last_match_pos -= old_storage_size / 2;
    } else {
        m_last_match_pos += old_storage_size / 2;
    }
}

template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::scan(ParserInputBuffer& input_buffer, Token& token)
        -> ErrorCode {
    auto const* state = m_dfa->get_root();
    if (m_asked_for_more_data) {
        state = m_prev_state;
        m_asked_for_more_data = false;
    } else {
        if (m_match) {
            m_match = false;
            m_last_match_pos = m_match_pos;
            m_last_match_line = m_match_line;
            token
                    = Token{m_start_pos,
                            m_match_pos,
                            input_buffer.storage().get_active_buffer(),
                            input_buffer.storage().size(),
                            m_match_line,
                            m_type_ids};
            return ErrorCode::Success;
        }
        m_start_pos = input_buffer.storage().pos();
        m_match_pos = input_buffer.storage().pos();
        m_match_line = m_line;
        m_type_ids = nullptr;
    }
    while (true) {
        auto prev_byte_buf_pos = input_buffer.storage().pos();
        auto next_char{utf8::cCharErr};
        if (auto const err = input_buffer.get_next_character(next_char); ErrorCode::Success != err)
        {
            m_asked_for_more_data = true;
            m_prev_state = state;
            return err;
        }
        if ((m_is_delimiter[next_char] || input_buffer.log_fully_consumed() || !m_has_delimiters)
            && state->is_accepting())
        {
            m_match = true;
            m_type_ids = &(state->get_matching_variable_ids());
            m_match_pos = prev_byte_buf_pos;
            m_match_line = m_line;
        }
        auto* next = state->next(next_char);
        if (next_char == '\n') {
            m_line++;
            if (m_has_delimiters && !m_match) {
                next = m_dfa->get_root()->next(next_char);
                m_match = true;
                m_type_ids = &(next->get_matching_variable_ids());
                m_start_pos = prev_byte_buf_pos;
                m_match_pos = input_buffer.storage().pos();
                m_match_line = m_line;
            }
        }
        if (input_buffer.log_fully_consumed() || next == nullptr) {
            if (m_match) {
                input_buffer.set_log_fully_consumed(false);
                input_buffer.set_pos(m_match_pos);
                m_line = m_match_line;
                if (m_last_match_pos != m_start_pos) {
                    token
                            = Token{m_last_match_pos,
                                    m_start_pos,
                                    input_buffer.storage().get_active_buffer(),
                                    input_buffer.storage().size(),
                                    m_last_match_line,
                                    &cTokenUncaughtStringTypes};
                    return ErrorCode::Success;
                }
                m_match = false;
                m_last_match_pos = m_match_pos;
                m_last_match_line = m_match_line;
                token
                        = Token{m_start_pos,
                                m_match_pos,
                                input_buffer.storage().get_active_buffer(),
                                input_buffer.storage().size(),
                                m_match_line,
                                m_type_ids};
                return ErrorCode::Success;
            }
            if (input_buffer.log_fully_consumed() && m_start_pos == input_buffer.storage().pos()) {
                if (m_last_match_pos != m_start_pos) {
                    m_match_pos = input_buffer.storage().pos();
                    m_type_ids = &cTokenEndTypes;
                    m_match = true;
                    token
                            = Token{m_last_match_pos,
                                    m_start_pos,
                                    input_buffer.storage().get_active_buffer(),
                                    input_buffer.storage().size(),
                                    m_last_match_line,
                                    &cTokenUncaughtStringTypes};
                    return ErrorCode::Success;
                }
                token
                        = Token{input_buffer.storage().pos(),
                                input_buffer.storage().pos(),
                                input_buffer.storage().get_active_buffer(),
                                input_buffer.storage().size(),
                                m_line,
                                &cTokenEndTypes};
                return ErrorCode::Success;
            }
            // TODO: remove timestamp from m_is_fist_char so that m_is_delimiter
            // check not needed
            while (input_buffer.log_fully_consumed() == false
                   && (m_is_first_char[next_char] == false || m_is_delimiter[next_char] == false))
            {
                prev_byte_buf_pos = input_buffer.storage().pos();
                if (ErrorCode err = input_buffer.get_next_character(next_char);
                    ErrorCode::Success != err)
                {
                    m_asked_for_more_data = true;
                    m_prev_state = state;
                    return err;
                }
            }
            input_buffer.set_pos(prev_byte_buf_pos);
            m_start_pos = prev_byte_buf_pos;
            state = m_dfa->get_root();
            continue;
        }
        state = next;
    }
}

// TODO: this is duplicating almost all the code of scan()
template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::scan_with_wildcard(
        ParserInputBuffer& input_buffer,
        char wildcard,
        Token& token
) -> ErrorCode {
    auto const* state = m_dfa->get_root();
    if (m_asked_for_more_data) {
        state = m_prev_state;
        m_asked_for_more_data = false;
    } else {
        if (m_match) {
            m_match = false;
            m_last_match_pos = m_match_pos;
            m_last_match_line = m_match_line;
            token
                    = Token{m_start_pos,
                            m_match_pos,
                            input_buffer.storage().get_active_buffer(),
                            input_buffer.storage().size(),
                            m_match_line,
                            m_type_ids};
            return ErrorCode::Success;
        }
        m_start_pos = input_buffer.storage().pos();
        m_match_pos = input_buffer.storage().pos();
        m_match_line = m_line;
        m_type_ids = nullptr;
    }
    while (true) {
        auto prev_byte_buf_pos = input_buffer.storage().pos();
        unsigned char next_char{utf8::cCharErr};
        if (ErrorCode err = input_buffer.get_next_character(next_char); ErrorCode::Success != err) {
            m_asked_for_more_data = true;
            m_prev_state = state;
            return err;
        }
        if ((m_is_delimiter[next_char] || input_buffer.log_fully_consumed() || !m_has_delimiters)
            && state->is_accepting())
        {
            m_match = true;
            m_type_ids = &(state->get_matching_variable_ids());
            m_match_pos = prev_byte_buf_pos;
            m_match_line = m_line;
        }
        TypedDfaState const* next = state->next(next_char);
        if (next_char == '\n') {
            m_line++;
            if (m_has_delimiters && !m_match) {
                next = m_dfa->get_root()->next(next_char);
                m_match = true;
                m_type_ids = &(next->get_matching_variable_ids());
                m_start_pos = prev_byte_buf_pos;
                m_match_pos = input_buffer.storage().pos();
                m_match_line = m_line;
            }
        }
        if (input_buffer.log_fully_consumed() || next == nullptr) {
            assert(input_buffer.log_fully_consumed());
            if (!m_match || (m_match && m_match_pos != input_buffer.storage().pos())) {
                token
                        = Token{m_last_match_pos,
                                input_buffer.storage().pos(),
                                input_buffer.storage().get_active_buffer(),
                                input_buffer.storage().size(),
                                m_last_match_line,
                                &cTokenUncaughtStringTypes};
                return ErrorCode::Success;
            }
            if (m_match) {
                // BFS (keep track of m_type_ids)
                if (wildcard == '?') {
                    for (uint32_t byte = 0; byte < cSizeOfByte; byte++) {
                        auto* next_state = state->next(byte);
                        if (next_state->is_accepting() == false) {
                            token
                                    = Token{m_last_match_pos,
                                            input_buffer.storage().pos(),
                                            input_buffer.storage().get_active_buffer(),
                                            input_buffer.storage().size(),
                                            m_last_match_line,
                                            &cTokenUncaughtStringTypes};
                            return ErrorCode::Success;
                        }
                    }
                } else if (wildcard == '*') {
                    std::stack<TypedDfaState const*> unvisited_states;
                    std::set<TypedDfaState const*> visited_states;
                    unvisited_states.push(state);
                    while (!unvisited_states.empty()) {
                        TypedDfaState const* current_state = unvisited_states.top();
                        if (current_state == nullptr || current_state->is_accepting() == false) {
                            token
                                    = Token{m_last_match_pos,
                                            input_buffer.storage().pos(),
                                            input_buffer.storage().get_active_buffer(),
                                            input_buffer.storage().size(),
                                            m_last_match_line,
                                            &cTokenUncaughtStringTypes};
                            return ErrorCode::Success;
                        }
                        unvisited_states.pop();
                        visited_states.insert(current_state);
                        for (uint32_t byte = 0; byte < cSizeOfByte; byte++) {
                            if (m_is_delimiter[byte]) {
                                continue;
                            }
                            TypedDfaState const* next_state = current_state->next(byte);
                            if (visited_states.find(next_state) == visited_states.end()) {
                                unvisited_states.push(next_state);
                            }
                        }
                    }
                }
                input_buffer.set_pos(m_match_pos);
                m_line = m_match_line;
                m_match = false;
                m_last_match_pos = m_match_pos;
                m_last_match_line = m_match_line;
                token
                        = Token{m_start_pos,
                                m_match_pos,
                                input_buffer.storage().get_active_buffer(),
                                input_buffer.storage().size(),
                                m_match_line,
                                m_type_ids};
                return ErrorCode::Success;
            }
        }
        state = next;
    }
}

template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::increase_buffer_capacity(ParserInputBuffer& input_buffer
) -> void {
    uint32_t old_storage_size{0};
    auto flipped_static_buffer{false};
    input_buffer.increase_capacity(old_storage_size, flipped_static_buffer);
    if (old_storage_size < input_buffer.storage().size()) {
        if (flipped_static_buffer) {
            flip_states(old_storage_size);
        }
        if (0 == m_last_match_pos) {
            m_last_match_pos = old_storage_size;
            m_start_pos = old_storage_size;
        }
    }
}

template <typename TypedNfaState, typename TypedDfaState>
void Lexer<TypedNfaState, TypedDfaState>::reset() {
    m_last_match_pos = 0;
    m_match = false;
    m_line = 0;
    m_match_pos = 0;
    m_start_pos = 0;
    m_match_line = 0;
    m_last_match_line = 0;
    m_type_ids = nullptr;
    m_asked_for_more_data = false;
    m_prev_state = nullptr;
}

template <typename TypedNfaState, typename TypedDfaState>
void Lexer<TypedNfaState, TypedDfaState>::prepend_start_of_file_char(ParserInputBuffer& input_buffer
) {
    m_prev_state = m_dfa->get_root()->next(utf8::cCharStartOfFile);
    m_asked_for_more_data = true;
    m_start_pos = input_buffer.storage().pos();
    m_match_pos = input_buffer.storage().pos();
    m_match_line = m_line;
    m_type_ids = nullptr;
}

template <typename TypedNfaState, typename TypedDfaState>
void Lexer<TypedNfaState, TypedDfaState>::add_delimiters(std::vector<uint32_t> const& delimiters) {
    assert(!delimiters.empty());
    m_has_delimiters = true;
    for (auto& i : m_is_delimiter) {
        i = false;
    }
    for (auto delimiter : delimiters) {
        m_is_delimiter[delimiter] = true;
    }
    m_is_delimiter[utf8::cCharStartOfFile] = true;
}

template <typename TypedNfaState, typename TypedDfaState>
void Lexer<TypedNfaState, TypedDfaState>::add_rule(
        uint32_t const& id,
        std::unique_ptr<finite_automata::RegexAST<TypedNfaState>> rule
) {
    m_rules.emplace_back(id, std::move(rule));
}

template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::get_rule(uint32_t const variable_id
) -> finite_automata::RegexAST<TypedNfaState>* {
    for (auto const& rule : m_rules) {
        if (rule.get_variable_id() == variable_id) {
            return rule.get_regex();
        }
    }
    return nullptr;
}

template <typename TypedNfaState, typename TypedDfaState>
void Lexer<TypedNfaState, TypedDfaState>::generate() {
    finite_automata::Nfa<TypedNfaState> nfa{std::move(m_rules)};
    // TODO: DFA ignores tags. E.g., treats "capture:user=(?<user_id>\d+)" as "capture:user=\d+"
    m_dfa = nfa_to_dfa(nfa);
    auto const* state = m_dfa->get_root();
    for (uint32_t i = 0; i < cSizeOfByte; i++) {
        if (state->next(i) != nullptr) {
            m_is_first_char[i] = true;
        } else {
            m_is_first_char[i] = false;
        }
    }
}

template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::epsilon_closure(TypedNfaState const* state_ptr
) -> std::set<TypedNfaState const*> {
    std::set<TypedNfaState const*> closure_set;
    std::stack<TypedNfaState const*> stack;
    stack.push(state_ptr);
    while (!stack.empty()) {
        auto const* current_state = stack.top();
        stack.pop();
        if (false == closure_set.insert(current_state).second) {
            continue;
        }
        for (auto const* dest_state : current_state->get_epsilon_transitions()) {
            stack.push(dest_state);
        }

        // TODO: currently treat tagged transitions as epsilon transitions
        for (auto const& positive_tagged_start_transition :
             current_state->get_positive_tagged_start_transitions())
        {
            stack.push(positive_tagged_start_transition.get_dest_state());
        }
        auto const& optional_positive_tagged_end_transition
                = current_state->get_positive_tagged_end_transition();
        if (optional_positive_tagged_end_transition.has_value()) {
            stack.push(optional_positive_tagged_end_transition.value().get_dest_state());
        }

        auto const& optional_negative_tagged_transition
                = current_state->get_negative_tagged_transition();
        if (optional_negative_tagged_transition.has_value()) {
            stack.push(optional_negative_tagged_transition.value().get_dest_state());
        }
    }
    return closure_set;
}

template <typename TypedNfaState, typename TypedDfaState>
auto Lexer<TypedNfaState, TypedDfaState>::nfa_to_dfa(finite_automata::Nfa<TypedNfaState>& nfa
) -> std::unique_ptr<finite_automata::Dfa<TypedDfaState>> {
    typedef std::set<TypedNfaState const*> StateSet;
    auto dfa = std::make_unique<finite_automata::Dfa<TypedDfaState>>();
    std::map<StateSet, TypedDfaState*> dfa_states;
    std::stack<StateSet> unmarked_sets;
    auto create_dfa_state
            = [&dfa, &dfa_states, &unmarked_sets](StateSet const& set) -> TypedDfaState* {
        auto* state = dfa->new_state(set);
        dfa_states[set] = state;
        unmarked_sets.push(set);
        return state;
    };
    auto start_set = epsilon_closure(nfa.get_root());
    create_dfa_state(start_set);
    while (!unmarked_sets.empty()) {
        auto set = unmarked_sets.top();
        unmarked_sets.pop();
        auto* dfa_state = dfa_states.at(set);
        std::map<uint32_t, StateSet> ascii_transitions_map;
        // map<Interval, StateSet> transitions_map;
        for (TypedNfaState const* s0 : set) {
            for (uint32_t i = 0; i < cSizeOfByte; i++) {
                for (TypedNfaState* const s1 : s0->get_byte_transitions(i)) {
                    StateSet closure = epsilon_closure(s1);
                    ascii_transitions_map[i].insert(closure.begin(), closure.end());
                }
            }
            // TODO: add this for the utf8 case
            /*
            for (const typename TypedNfaState::Tree::Data& data : s0->get_tree_transitions().all())
            { for (TypedNfaState* const s1 : data.m_value) { StateSet closure = epsilon_closure(s1);
                    transitions_map[data.m_interval].insert(closure.begin(), closure.end());
                }
            }
            */
        }
        auto next_dfa_state
                = [&dfa_states, &create_dfa_state](StateSet const& set) -> TypedDfaState* {
            TypedDfaState* state{nullptr};
            auto it = dfa_states.find(set);
            if (it == dfa_states.end()) {
                state = create_dfa_state(set);
            } else {
                state = it->second;
            }
            return state;
        };
        for (typename std::map<uint32_t, StateSet>::value_type const& kv : ascii_transitions_map) {
            auto* dest_state = next_dfa_state(kv.second);
            dfa_state->add_byte_transition(kv.first, dest_state);
        }
        // TODO: add this for the utf8 case
        /*
        for (const typename map<Interval, typename TypedNfaState::StateSet>::value_type& kv :
             transitions_map)
        {
            TypedDfaState* dest_state = next_dfa_state(kv.second);
            dfa_state->add_tree_transition(kv.first, dest_state);
        }
        */
    }
    return dfa;
}
}  // namespace log_surgeon

#endif  // LOG_SURGEON_LEXER_TPP
