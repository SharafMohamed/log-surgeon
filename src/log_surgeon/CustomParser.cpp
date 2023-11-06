#include "CustomParser.hpp"

#include <memory>
#include <span>
#include <stdexcept>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/LALR1Parser.hpp>

using std::make_unique;
using std::string;
using std::unique_ptr;

namespace log_surgeon {

auto CustomParser::init() -> void {
    add_lexical_rules();
    add_productions();
    generate();
}

auto CustomParser::clear() -> void {
    // TODO: this should be done in parse() as a reset() function
    // Can reset the buffers at this point and allow reading
    if (NonTerminal::m_next_children_start > cSizeOfAllChildren / 2) {
        NonTerminal::m_next_children_start = 0;
    }
    clear_child();
}

auto CustomParser::parse_input(std::string const& input_string) -> std::unique_ptr<ParserAST> {
    Reader reader{[&](char* dst_buf, size_t count, size_t& read_to) -> ErrorCode {
        uint32_t unparsed_string_pos = 0;
        std::span<char> const buf{dst_buf, count};
        if (unparsed_string_pos + count > input_string.length()) {
            count = input_string.length() - unparsed_string_pos;
        }
        read_to = count;
        if (read_to == 0) {
            return ErrorCode::EndOfFile;
        }
        for (uint32_t i = 0; i < count; i++) {
            buf[i] = input_string[unparsed_string_pos + i];
        }
        unparsed_string_pos += count;
        return ErrorCode::Success;
    }};

    clear();
    // TODO: for super long strings (10000+ tokens) parse can currently crash
    NonTerminal nonterminal = parse(reader);
    return std::move(nonterminal.get_parser_ast());
}
}  // namespace log_surgeon
