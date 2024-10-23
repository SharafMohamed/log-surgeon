#ifndef LOG_SURGEON_LEXICAL_RULE_HPP
#define LOG_SURGEON_LEXICAL_RULE_HPP
#include <cstdint>
#include <memory>

#include <log_surgeon/finite_automata/RegexAST.hpp>

namespace log_surgeon {
template <typename NFAStateType>
class LexicalRule {
public:
    // Constructor
    LexicalRule(
            uint32_t const variable_id,
            std::unique_ptr<finite_automata::RegexAST<NFAStateType>> regex
    )
            : m_variable_id(variable_id),
              m_regex(std::move(regex)) {}

    /**
     * Adds AST representing the lexical rule to the NFA
     * @param nfa
     */
    auto add_to_nfa(finite_automata::RegexNFA<NFAStateType>* nfa) const -> void;

    [[nodiscard]] auto get_variable_id() const -> uint32_t { return m_variable_id; }

    [[nodiscard]] auto get_regex() const -> finite_automata::RegexAST<NFAStateType>* {
        // TODO: make the returned pointer constant
        return m_regex.get();
    }

private:
    uint32_t m_variable_id;
    std::unique_ptr<finite_automata::RegexAST<NFAStateType>> m_regex;
};

template <typename NFAStateType>
void LexicalRule<NFAStateType>::add_to_nfa(finite_automata::RegexNFA<NFAStateType>* nfa) const {
    auto* end_state = nfa->new_state();
    end_state->set_accepting(true);
    end_state->set_matching_variable_id(m_variable_id);
    m_regex->add_to_nfa_with_negative_tags(nfa, end_state);
}
}  // namespace log_surgeon

#endif  // LOG_SURGEON_LEXICAL_RULE_HPP
