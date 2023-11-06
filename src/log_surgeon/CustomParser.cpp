#include "CustomParser.hpp"

#include <memory>
#include <span>
#include <stdexcept>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>
#include <log_surgeon/LALR1Parser.hpp>
#include <log_surgeon/Schema.hpp>

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
    auto value = make_unique<JsonValueAST>(std::move(r2));
    value->m_view_start_pos = m->token_cast(0)->m_start_pos;
    value->m_view_end_pos = m->token_cast(4)->m_end_pos;
    value->m_view_buffer = m->token_cast(4)->m_buffer;
    return value;
}

static auto dict_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r2 = m->non_terminal_cast(1)->get_parser_ast();
    auto recordAST = make_unique<JsonRecordAST>(r2);
    auto value = make_unique<JsonValueAST>(std::move(recordAST));
    value->m_view_start_pos = m->token_cast(0)->m_start_pos;
    value->m_view_end_pos = m->token_cast(3)->m_end_pos;
    value->m_view_buffer = m->token_cast(3)->m_buffer;
    return value;
}

static auto empty_dictionary_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    auto value = make_unique<JsonValueAST>(nullptr);
    value->m_view_start_pos = m->token_cast(0)->m_start_pos;
    value->m_view_end_pos = m->token_cast(2)->m_end_pos;
    value->m_view_buffer = m->token_cast(2)->m_buffer;
    return value;
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
    Schema schema;
    schema.add_variable("spacePlus", " +", -1);
    schema.add_variable("lBrace", "\\{", -1);
    schema.add_variable("rBrace", "\\}", -1);
    schema.add_variable("comma", ",", -1);
    schema.add_variable("equal", "=", -1);
    schema.add_variable("integer", "[0-9]+", -1);
    schema.add_variable("boolean", "true|false", -1);
    schema.add_variable("string", R"(([^ \{\},=])|([^ \{\},=][^,=]*[^ \{\},=]))", -1);
    for (auto const& parser_ast : schema.get_schema_ast_ptr()->m_schema_vars) {
        auto* schema_var_ast = dynamic_cast<SchemaVarAST*>(parser_ast.get());
        add_rule(schema_var_ast->m_name, std::move(schema_var_ast->m_regex_ptr));
    }
}

// " request and response, importance=high, this is some text, status=low, memory=10GB"
void CustomParser::add_productions() {
    add_production("Record", {"Record", "GoodObject", "SpaceStar", "comma"}, existing_record_rule);
    add_production("Record", {"Record", "BadObject", "SpaceStar", "comma"}, existing_record_rule);
    add_production("Record", {"Record", "GoodObject", "SpaceStar", "$end"}, existing_record_rule);
    add_production("Record", {"Record", "BadObject", "SpaceStar", "$end"}, existing_record_rule);
    add_production("Record", {"GoodObject", "SpaceStar", "comma"}, new_record_rule);
    add_production("Record", {"BadObject", "SpaceStar", "comma"}, new_record_rule);
    add_production("Record", {"GoodObject", "SpaceStar", "$end"}, new_record_rule);
    add_production("Record", {"BadObject", "SpaceStar", "$end"}, new_record_rule);
    add_production("GoodObject", {"GoodObject", "SpaceStar", "equal"}, char_object_rule);
    add_production("GoodObject", {"GoodObject", "SpaceStar", "Value"}, existing_object_rule);
    add_production("GoodObject", {"GoodObject", "SpaceStar", "Value"}, existing_object_rule);
    add_production("GoodObject", {"BadObject", "SpaceStar", "equal"}, new_good_object_rule);
    add_production(
            "BadObject",
            {"SpaceStar", "Value"},
            std::bind(&CustomParser::bad_object_rule, this, std::placeholders::_1)
    );
    add_production("Value", {"string"}, new_string_rule);
    add_production(
            "Value",
            {"lBrace", "Record", "GoodObject", "SpaceStar", "rBrace"},
            dict_object_rule
    );
    add_production("Value", {"lBrace", "GoodObject", "SpaceStar", "rBrace"}, dict_record_rule);
    add_production(
            "Value",
            {"lBrace", "Record", "BadObject", "SpaceStar", "rBrace"},
            dict_object_rule
    );
    add_production("Value", {"lBrace", "BadObject", "SpaceStar", "rBrace"}, dict_record_rule);
    add_production("Value", {"lBrace", "SpaceStar", "rBrace"}, empty_dictionary_rule);
    add_production("Value", {"boolean"}, boolean_rule);
    add_production("Value", {"integer"}, integer_rule);
    add_production("Value", {"integer"}, integer_rule);
    add_production("SpaceStar", {"spacePlus"}, new_string_rule);
    add_production("SpaceStar", {}, empty_string_rule);
}
}  // namespace log_surgeon
