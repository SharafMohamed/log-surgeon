#include "CustomParser.hpp"

#include <cmath>
#include <memory>
#include <span>
#include <stdexcept>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>
#include <log_surgeon/LALR1Parser.hpp>
#include <log_surgeon/Lexer.hpp>
#include <log_surgeon/utils.hpp>

using RegexASTGroupByte = log_surgeon::finite_automata::RegexASTGroup<
        log_surgeon::finite_automata::RegexNFAByteState>;
using RegexASTLiteralByte = log_surgeon::finite_automata::RegexASTLiteral<
        log_surgeon::finite_automata::RegexNFAByteState>;
using RegexASTMultiplicationByte = log_surgeon::finite_automata::RegexASTMultiplication<
        log_surgeon::finite_automata::RegexNFAByteState>;
using RegexASTCatByte = log_surgeon::finite_automata::RegexASTCat<
        log_surgeon::finite_automata::RegexNFAByteState>;
using RegexASTOrByte
        = log_surgeon::finite_automata::RegexASTOr<log_surgeon::finite_automata::RegexNFAByteState>;
using std::make_unique;
using std::string;
using std::unique_ptr;

namespace log_surgeon {
JsonValueAST::JsonValueAST(
        uint32_t view_start_pos,
        uint32_t view_end_pos,
        char const* view_buffer,
        JsonValueType type
)
        : m_view_start_pos(view_start_pos),
          m_view_end_pos(view_end_pos),
          m_view_buffer(view_buffer),
          m_type(type),
          m_dictionary_json_record(nullptr) {}

JsonValueAST::JsonValueAST(std::unique_ptr<ParserAST> json_record_ast)
        : m_type(JsonValueType::Dictionary),
          m_dictionary_json_record(std::move(json_record_ast)) {}

auto JsonValueAST::print(bool with_types) -> std::string {
    std::string output;
    if (with_types) {
        output += "<";
        output += print_json_type(m_type);
        output += ">";
    }
    if (m_type == JsonValueType::Dictionary) {
        if (m_dictionary_json_record != nullptr) {
            auto* json_record_ast = dynamic_cast<JsonRecordAST*>(m_dictionary_json_record.get());
            output += json_record_ast->print(false);
        }
    } else if (m_type == JsonValueType::String) {
        // TODO: this is a hacky fix for now to deal with dictionaries turned into strings
        if (m_dictionary_json_record != nullptr) {
            auto* json_record_ast = dynamic_cast<JsonRecordAST*>(m_dictionary_json_record.get());
            output += json_record_ast->print(false);
        }
        output += to_string_view();
    } else {
        output += to_string_view();
    }
    return output;
}

static auto boolean_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    return make_unique<JsonValueAST>(
            m->token_cast(0)->m_start_pos,
            m->token_cast(0)->m_end_pos,
            m->token_cast(0)->m_buffer,
            JsonValueType::Boolean
    );
}

static auto empty_string_rule(NonTerminal*) -> unique_ptr<ParserAST> {
    return make_unique<JsonValueAST>(0, 0, nullptr, JsonValueType::String);
}

static auto new_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    return make_unique<JsonValueAST>(
            m->token_cast(0)->m_start_pos,
            m->token_cast(0)->m_end_pos,
            m->token_cast(0)->m_buffer,
            JsonValueType::String
    );
}

static auto integer_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    return make_unique<JsonValueAST>(
            m->token_cast(0)->m_start_pos,
            m->token_cast(0)->m_end_pos,
            m->token_cast(0)->m_buffer,
            JsonValueType::Integer
    );
}

static auto dict_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r2 = m->non_terminal_cast(1)->get_parser_ast();
    auto* r2_ptr = dynamic_cast<JsonRecordAST*>(r2.get());
    unique_ptr<ParserAST>& r3 = m->non_terminal_cast(2)->get_parser_ast();
    r2_ptr->add_object_ast(r3);
    return make_unique<JsonValueAST>(std::move(r2));
}

static auto dict_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r2 = m->non_terminal_cast(1)->get_parser_ast();
    auto recordAST = make_unique<JsonRecordAST>(r2);
    return make_unique<JsonValueAST>(std::move(recordAST));
}

static auto empty_dictionary_rule(NonTerminal*) -> unique_ptr<ParserAST> {
    return make_unique<JsonValueAST>(nullptr);
}

auto CustomParser::bad_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r2 = m->non_terminal_cast(1)->get_parser_ast();
    return make_unique<JsonObjectAST>(m_bad_key_counter, r2);
}

static auto new_good_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonObjectAST*>(r1.get());
    auto* value_ast = dynamic_cast<JsonValueAST*>(r1_ptr->m_value_ast.get());
    unique_ptr<ParserAST> empty_value
            = make_unique<JsonValueAST>(0, 0, nullptr, JsonValueType::String);
    return make_unique<JsonObjectAST>(value_ast->to_string_view(), empty_value);
}

static auto existing_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonObjectAST*>(r1.get());
    auto* value_ast = dynamic_cast<JsonValueAST*>(r1_ptr->m_value_ast.get());
    unique_ptr<ParserAST>& r3 = m->non_terminal_cast(2)->get_parser_ast();
    auto* r3_ptr = dynamic_cast<JsonValueAST*>(r3.get());
    if (nullptr == value_ast->m_view_buffer && value_ast->m_type == JsonValueType::String) {
        r1_ptr->m_value_ast = std::move(r3);
    } else {
        value_ast->change_type(JsonValueType::String);
        value_ast->m_view_end_pos = r3_ptr->m_view_end_pos;
    }
    return std::move(r1);
}

static auto char_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonObjectAST*>(r1.get());
    auto* value_ast = dynamic_cast<JsonValueAST*>(r1_ptr->m_value_ast.get());
    value_ast->change_type(JsonValueType::String);
    value_ast->m_view_end_pos = m->token_cast(2)->m_end_pos;
    return std::move(r1);
}

static auto identity_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    return std::move(r1);
}

static auto new_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    return make_unique<JsonRecordAST>(r1);
}

static auto existing_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonRecordAST*>(r1.get());
    unique_ptr<ParserAST>& r2 = m->non_terminal_cast(1)->get_parser_ast();
    r1_ptr->add_object_ast(r2);
    return std::move(r1);
}

CustomParser::CustomParser() {
    add_lexical_rules();
    add_productions();
    generate();
}

auto CustomParser::parse_input(std::string const& json_like_string) -> std::unique_ptr<ParserAST> {
    Reader reader{[&](char* dst_buf, size_t count, size_t& read_to) -> ErrorCode {
        uint32_t unparsed_string_pos = 0;
        std::span<char> const buf{dst_buf, count};
        if (unparsed_string_pos + count > json_like_string.length()) {
            count = json_like_string.length() - unparsed_string_pos;
        }
        read_to = count;
        if (read_to == 0) {
            return ErrorCode::EndOfFile;
        }
        for (uint32_t i = 0; i < count; i++) {
            buf[i] = json_like_string[unparsed_string_pos + i];
        }
        unparsed_string_pos += count;
        return ErrorCode::Success;
    }};

    // TODO: this should be done in parse() as a reset() function
    // Can reset the buffers at this point and allow reading
    if (NonTerminal::m_next_children_start > cSizeOfAllChildren / 2) {
        NonTerminal::m_next_children_start = 0;
    }
    m_bad_key_counter = 0;
    // TODO: for super long strings (10000+ tokens) parse can currently crash
    NonTerminal nonterminal = parse(reader);
    return std::move(nonterminal.get_parser_ast());
}

void CustomParser::add_lexical_rules() {
    add_rule(
            "spacePlus",
            make_unique<RegexASTMultiplicationByte>(make_unique<RegexASTLiteralByte>(' '), 1, 0)
    );
    add_token("lBrace", '{');
    add_token("rBrace", '}');
    add_token("comma", ',');
    add_token("equal", '=');
    auto digit = make_unique<RegexASTGroupByte>('0', '9');
    auto digit_plus = make_unique<RegexASTMultiplicationByte>(std::move(digit), 1, 0);
    add_rule("integer", std::move(digit_plus));
    add_token_chain("boolean", "true");
    add_token_chain("boolean", "false");
    // default constructs to a m_negate group
    auto single_char = make_unique<RegexASTGroupByte>();
    single_char->add_literal(',');
    single_char->add_literal('=');
    single_char->add_literal(' ');
    single_char->add_literal('{');
    single_char->add_literal('}');
    auto string_infix = make_unique<RegexASTGroupByte>();
    string_infix->add_literal(',');
    string_infix->add_literal('=');
    auto string_prefix = make_unique<RegexASTGroupByte>();
    string_prefix->add_literal(',');
    string_prefix->add_literal('=');
    string_prefix->add_literal(' ');
    string_prefix->add_literal('{');
    string_prefix->add_literal('}');
    auto string_suffix = make_unique<RegexASTGroupByte>();
    string_suffix->add_literal(',');
    string_suffix->add_literal('=');
    string_suffix->add_literal(' ');
    string_suffix->add_literal('{');
    string_suffix->add_literal('}');
    auto string_character_plus
            = make_unique<RegexASTMultiplicationByte>(std::move(string_infix), 1, 0);
    auto multi_char_string_prefix = make_unique<RegexASTCatByte>(
            std::move(string_prefix),
            std::move(string_character_plus)
    );
    auto multi_char_string = make_unique<RegexASTCatByte>(
            std::move(multi_char_string_prefix),
            std::move(string_suffix)
    );
    auto any_string
            = make_unique<RegexASTOrByte>(std::move(single_char), std::move(multi_char_string));
    add_rule("string", std::move(any_string));
}

// " request and response, importance=high, this is some text, status=low, memory=10GB"
void CustomParser::add_productions() {
    add_production("Record", {"Record", "FinishedObject"}, existing_record_rule);
    add_production("Record", {"FinishedObject"}, new_record_rule);
    add_production("FinishedObject", {"GoodObject", "SpaceStar", "comma"}, identity_rule);
    add_production("FinishedObject", {"NewGoodObject", "SpaceStar", "comma"}, identity_rule);
    add_production("FinishedObject", {"BadObject", "SpaceStar", "comma"}, identity_rule);
    add_production(
            "FinishedObject",
            {"NewGoodObject", "SpaceStar", "Dictionary", "SpaceStar", "comma"},
            identity_rule
    );
    add_production("FinishedObject", {"GoodObject", "SpaceStar", "$end"}, identity_rule);
    add_production("FinishedObject", {"NewGoodObject", "SpaceStar", "$end"}, identity_rule);
    add_production("FinishedObject", {"BadObject", "SpaceStar", "$end"}, identity_rule);
    add_production(
            "FinishedObject",
            {"NewGoodObject", "SpaceStar", "Dictionary", "SpaceStar", "$end"},
            identity_rule
    );
    add_production("GoodObject", {"GoodObject", "SpaceStar", "equal"}, char_object_rule);
    add_production("GoodObject", {"GoodObject", "SpaceStar", "Value"}, existing_object_rule);
    add_production("GoodObject", {"NewGoodObject", "SpaceStar", "equal"}, char_object_rule);
    add_production("GoodObject", {"NewGoodObject", "SpaceStar", "Value"}, existing_object_rule);
    add_production("NewGoodObject", {"BadObject", "SpaceStar", "equal"}, new_good_object_rule);
    add_production(
            "BadObject",
            {"SpaceStar", "Value"},
            std::bind(&CustomParser::bad_object_rule, this, std::placeholders::_1)
    );
    add_production("Value", {"string"}, new_string_rule);
    add_production("Dictionary", {"lBrace", "Record", "FinishedBraceObject"}, dict_object_rule);
    add_production("Dictionary", {"lBrace", "FinishedBraceObject"}, dict_record_rule);
    add_production("Dictionary", {"lBrace", "SpaceStar", "rBrace"}, empty_dictionary_rule);
    add_production("FinishedBraceObject", {"GoodObject", "SpaceStar", "rBrace"}, identity_rule);
    add_production("FinishedBraceObject", {"BadObject", "SpaceStar", "rBrace"}, identity_rule);
    add_production("Value", {"boolean"}, boolean_rule);
    add_production("Value", {"integer"}, integer_rule);
    add_production("Value", {"integer"}, integer_rule);
    add_production("SpaceStar", {"spacePlus"}, new_string_rule);
    add_production("SpaceStar", {}, empty_string_rule);
}
}  // namespace log_surgeon
