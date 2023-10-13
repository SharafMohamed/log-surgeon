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
using RegexASTMultiplicationByte = log_surgeon::finite_automata::RegexASTMultiplication<
        log_surgeon::finite_automata::RegexNFAByteState>;

using std::make_unique;
using std::string;
using std::unique_ptr;

namespace log_surgeon {
static auto boolean_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    return make_unique<JsonValueAST>(m->token_cast(0)->to_string(), JsonValueType::Boolean);
}

static auto new_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    string r0 = m->token_cast(0)->to_string();
    return make_unique<JsonValueAST>(r0, JsonValueType::String);
}

static auto integer_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    string r0 = m->token_cast(0)->to_string();
    return make_unique<JsonValueAST>(r0, JsonValueType::Integer);
}

static auto existing_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonValueAST*>(r1.get());
    string r2 = m->token_cast(1)->to_string();
    r1_ptr->add_string(r2);
    return std::move(r1);
}

static auto swap_to_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonValueAST*>(r1.get());
    string r2 = m->token_cast(1)->to_string();
    r1_ptr->change_type(JsonValueType::String);
    r1_ptr->add_string(r2);
    return std::move(r1);
}

static auto value_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    return std::move(r1);
}

auto CustomParser::bad_json_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    return make_unique<JsonObjectAST>(m_bad_key_counter, r1);
}

static auto existing_good_json_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonObjectAST*>(r1.get());
    unique_ptr<ParserAST>& r2 = m->non_terminal_cast(1)->get_parser_ast();
    r1_ptr->set_value(r2);
    return std::move(r1);
}

static auto good_json_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonValueAST*>(r1.get());
    std::unique_ptr<ParserAST> empty_json_value_AST
            = make_unique<JsonValueAST>("", JsonValueType::String);
    return make_unique<JsonObjectAST>(r1_ptr->get_value(), empty_json_value_AST);
}

static auto new_json_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    return make_unique<JsonRecordAST>(r1);
}

static auto existing_json_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonRecordAST*>(r1.get());
    unique_ptr<ParserAST>& r3 = m->non_terminal_cast(2)->get_parser_ast();
    r1_ptr->add_object_ast(r3);
    return std::move(r1);
}

CustomParser::CustomParser() {
    add_lexical_rules();
    add_productions();
    generate();
}

auto CustomParser::parse_json_like_string(std::string const& json_like_string)
        -> std::unique_ptr<JsonRecordAST> {
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
    std::unique_ptr<JsonRecordAST> json_record_ast(
            dynamic_cast<JsonRecordAST*>(nonterminal.get_parser_ast().release())
    );
    return json_record_ast;
}

void CustomParser::add_lexical_rules() {
    // add_token("Quotation", '"');
    add_token("Comma", ',');
    add_token("Equal", '=');
    auto digit = make_unique<RegexASTGroupByte>('0', '9');
    auto digit_plus = make_unique<RegexASTMultiplicationByte>(std::move(digit), 1, 0);
    add_rule("IntegerToken", std::move(digit_plus));
    // add_token("Lbracket", '[');
    // add_token("Rbracket", ']');
    // add_token("Lbrace", '{');
    // add_token("Rbrace", '}');
    add_token_chain("BooleanToken", "true");
    add_token_chain("BooleanToken", "false");
    // default constructs to a m_negate group
    unique_ptr<RegexASTGroupByte> string_character = make_unique<RegexASTGroupByte>();
    string_character->add_literal(',');
    string_character->add_literal('=');
    unique_ptr<RegexASTMultiplicationByte> string_character_plus
            = make_unique<RegexASTMultiplicationByte>(std::move(string_character), 1, 0);
    add_rule("StringToken", std::move(string_character_plus));
}

// " request and response, importance=high, this is some text, status=low, memory=10GB"
void CustomParser::add_productions() {
    add_production("JsonRecord", {"JsonRecord", "Comma", "JsonObject"}, existing_json_record_rule);
    add_production(
            "JsonRecord",
            {"JsonRecord", "Comma", "GoodJsonObject"},
            existing_json_record_rule
    );
    add_production("JsonRecord", {"JsonObject"}, new_json_record_rule);
    add_production("JsonRecord", {"GoodJsonObject"}, new_json_record_rule);

    add_production("JsonObject", {"GoodJsonObject", "Value"}, existing_good_json_object_rule);
    add_production("GoodJsonObject", {"String", "Equal"}, good_json_object_rule);
    add_production(
            "JsonObject",
            {"String"},
            std::bind(&CustomParser::bad_json_object_rule, this, std::placeholders::_1)
    );

    // add_production("Value", {"CompleteList"}, value_rule);
    // add_production("Value", {"Dict"}, value_rule);
    add_production("Value", {"Boolean"}, value_rule);
    add_production("Value", {"Integer"}, value_rule);
    add_production("Value", {"String"}, value_rule);
    add_production("Value", {"EqualString"}, value_rule);

    // todo: for now we treat list as a string
    // add_production("CompleteList", {"IncompleteList", "Rbracket"}, finished_list_rule);
    // add_production("IncompleteList", {"Lbracket", "Value"}, new_list_rule);
    // add_production("IncompleteList", {"List", "Comma", "Value"}, existing_list_rule);

    // todo: for now we treat dictionary as a string
    // add_production("CompleteDict", {"IncompleteDict", "Rbrace"}, finished_dictionary_rule);
    // add_production("IncompleteDict", {"Lbrace", "Pair"}, new_dictionary_rule);
    // add_production("IncompleteDict", {"List", "Comma", "Pair"}, existing_dictionary_rule);
    // add_production("Pair", {"String", "Colon", "Value"}, pair_rule);

    add_production("EqualString", {"Equal"}, new_string_rule);
    add_production("EqualString", {"String", "Equal"}, existing_string_rule);
    add_production("EqualString", {"Integer", "Equal"}, swap_to_string_rule);
    add_production("EqualString", {"Boolean", "Equal"}, swap_to_string_rule);
    add_production("EqualString", {"EqualString", "Equal"}, existing_string_rule);
    add_production("EqualString", {"EqualString", "StringToken"}, existing_string_rule);

    add_production("String", {"StringToken"}, new_string_rule);
    add_production("Integer", {"IntegerToken"}, integer_rule);
    add_production("Boolean", {"BooleanToken"}, boolean_rule);
}
}  // namespace log_surgeon
