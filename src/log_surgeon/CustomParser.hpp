#ifndef LOG_SURGEON_CUSTOM_PARSER_HPP
#define LOG_SURGEON_CUSTOM_PARSER_HPP

#include <utility>

#include <log_surgeon/LALR1Parser.hpp>

namespace log_surgeon {

class CustomParser : public LALR1Parser<
                             finite_automata::RegexNFAByteState,
                             finite_automata::RegexDFAByteState> {
public:
    // Constructor
    CustomParser() = default;

    auto init() -> void;

    auto clear() -> void;

    auto parse_input(std::string const& json_like_string) -> std::unique_ptr<ParserAST>;

private:

    /**
     * Clears anything needed by child between parse calls
     */
    virtual void clear_child() = 0;

    /**
     * Add all lexical rules needed for json lexing
     */
    virtual void add_lexical_rules() = 0;

    /**
     * Add all productions needed for json parsing
     */
    virtual void add_productions() = 0;

};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_CUSTOM_PARSER_HPP
