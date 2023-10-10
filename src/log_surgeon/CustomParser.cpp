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

using std::make_unique;
using std::string;
using std::unique_ptr;

namespace log_surgeon {
static auto boolean_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    string r0 = m->token_cast(0)->to_string();
    return make_unique<JsonValueAST>(r0, JsonValueType::Boolean);
}

static auto new_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    string r0 = m->token_cast(0)->to_string();
    return make_unique<JsonValueAST>(r0, JsonValueType::String);
}

static auto new_integer_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    string r0 = m->token_cast(0)->to_string();
    return make_unique<JsonValueAST>(r0, JsonValueType::Integer);
}

static auto existing_integer_or_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonValueAST*>(r1.get());
    string r2 = m->token_cast(1)->to_string();
    r1_ptr->add_character(r2[0]);
    return std::move(r1);
}

static auto swap_to_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonValueAST*>(r1.get());
    string r2 = m->token_cast(1)->to_string();
    r1_ptr->change_type(JsonValueType::String);
    r1_ptr->add_character(r2[0]);
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

static auto good_json_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonValueAST*>(r1.get());
    unique_ptr<ParserAST>& r3 = m->non_terminal_cast(2)->get_parser_ast();
    return make_unique<JsonObjectAST>(r1_ptr->get_value(), r3);
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
    m_json_tree_root.key = "root";
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
    add_token_group("Numeric", make_unique<RegexASTGroupByte>('0', '9'));
    // add_token("Lbracket", '[');
    // add_token("Rbracket", ']');
    // add_token("Lbrace", '{');
    // add_token("Rbrace", '}');
    add_token_chain("True", "true");
    add_token_chain("False", "false");
    // default constructs to a m_negate group
    unique_ptr<RegexASTGroupByte> string_character = make_unique<RegexASTGroupByte>();
    string_character->add_literal(',');
    string_character->add_literal('=');
    add_token_group("StringCharacter", std::move(string_character));
}

// " request and response, importance=high, this is some text, status=low, memory=10GB"
void CustomParser::add_productions() {
    add_production("JsonRecord", {"JsonRecord", "Comma", "JsonObject"}, existing_json_record_rule);
    add_production("JsonRecord", {"JsonObject"}, new_json_record_rule);

    add_production("JsonObject", {"String", "Equal", "Value"}, good_json_object_rule);
    add_production(
            "JsonObject",
            {"String"},
            std::bind(&CustomParser::bad_json_object_rule, this, std::placeholders::_1)
    );

    // add_production("Value", {"CompleteList"}, value_rule);
    // add_production("Value", {"Dict"}, value_rule);
    add_production("Value", {"Boolean"}, value_rule);
    add_production("Value", {"String"}, value_rule);
    add_production("Value", {"Integer"}, value_rule);

    // todo: for now we treat list as a string
    // add_production("CompleteList", {"IncompleteList", "Rbracket"}, finished_list_rule);
    // add_production("IncompleteList", {"Lbracket", "Value"}, new_list_rule);
    // add_production("IncompleteList", {"List", "Comma", "Value"}, existing_list_rule);

    // todo: for now we treat dictionary as a string
    // add_production("CompleteDict", {"IncompleteDict", "Rbrace"}, finished_dictionary_rule);
    // add_production("IncompleteDict", {"Lbrace", "Pair"}, new_dictionary_rule);
    // add_production("IncompleteDict", {"List", "Comma", "Pair"}, existing_dictionary_rule);
    // add_production("Pair", {"String", "Colon", "Value"}, pair_rule);

    add_production("String", {"String", "StringCharacter"}, existing_integer_or_string_rule);
    add_production("String", {"Integer", "StringCharacter"}, swap_to_string_rule);
    add_production("String", {"Boolean", "StringCharacter"}, swap_to_string_rule);
    add_production("String", {"StringCharacter"}, new_string_rule);

    add_production("Boolean", {"True"}, boolean_rule);
    add_production("Boolean", {"False"}, boolean_rule);

    add_production("Integer", {"Integer", "Numeric"}, existing_integer_or_string_rule);
    add_production("Integer", {"Numeric"}, new_integer_rule);
}
}  // namespace log_surgeon
