#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <log_surgeon/CustomParser.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/LALR1Parser.hpp>
#include <log_surgeon/Schema.hpp>

using std::make_unique;
using std::string;
using std::unique_ptr;

using namespace log_surgeon;

namespace log_surgeon {

enum class JsonValueType {
    Integer,
    Boolean,
    String,
    Dictionary,
    List
};

inline std::string print_json_type(JsonValueType json_value_type) {
    switch (json_value_type) {
        case JsonValueType::Integer:
            return "integer";
        case JsonValueType::Boolean:
            return "boolean";
        case JsonValueType::String:
            return "string";
        case JsonValueType::Dictionary:
            return "dictionary";
        case JsonValueType::List:
            return "list";
            // omit default case to trigger compiler warning for missing cases
    }
    return "invalid";
}

// ASTs used in CustomParser AST
class JsonValueAST : public ParserAST {
public:
    // Constructor
    JsonValueAST(
            uint32_t view_start_pos,
            uint32_t view_end_pos,
            char const* view_buffer,
            JsonValueType type
    );

    JsonValueAST(std::unique_ptr<ParserAST> json_record_ast);

    auto change_type(JsonValueType type) -> void { m_type = type; }

    [[nodiscard]] auto to_string_view() -> std::string_view {
        return {m_view_buffer + m_view_start_pos, m_view_end_pos - m_view_start_pos};
    }

    auto print(bool with_types) -> std::string;

    uint32_t m_view_start_pos{0};
    uint32_t m_view_end_pos{0};
    char const* m_view_buffer{nullptr};
    JsonValueType m_type;
    std::unique_ptr<ParserAST> m_dictionary_json_record;
};

class JsonObjectAST : public ParserAST {
public:
    // Constructor
    JsonObjectAST(std::string_view key, std::unique_ptr<ParserAST>& value_ast)
            : m_key(key),
              m_value_ast(std::move(value_ast)) {}

    JsonObjectAST(uint32_t& bad_key_counter, std::unique_ptr<ParserAST>& value_ast)
            : m_bad_key("key" + std::to_string(bad_key_counter++)),
              m_key(m_bad_key),
              m_value_ast(std::move(value_ast)) {}

    auto print(bool with_types) -> std::string {
        std::string output = "\"" + std::string(m_key) + "\"";
        output += ":";
        auto* value_ptr = static_cast<JsonValueAST*>(m_value_ast.get());
        output += value_ptr->print(with_types);
        return output;
    }

    std::string m_bad_key{};
    std::string_view const m_key;
    std::unique_ptr<ParserAST> m_value_ast;
};

class JsonRecordAST : public ParserAST {
public:
    // Constructor
    JsonRecordAST(std::unique_ptr<ParserAST>& object_ast) { add_object_ast(object_ast); }

    auto add_object_ast(std::unique_ptr<ParserAST>& object_ast) -> void {
        m_object_asts.push_back(std::move(object_ast));
    }

    auto print(bool with_types = false) -> std::string {
        std::string output{};
        for (auto const& object_ast : m_object_asts) {
            auto* object_ptr = dynamic_cast<JsonObjectAST*>(object_ast.get());
            output += object_ptr->print(with_types);
            output += ",";
        }
        output.pop_back();
        return output;
    }

    std::vector<std::unique_ptr<ParserAST>> m_object_asts;
};

class JsonLikeParser : public CustomParser {
public:
    // Constructor
    JsonLikeParser();

private:
    /**
     * Clears anything needed by child between parse calls
     */
    auto clear_child() -> void;

    /**
     * Add all lexical rules needed for json lexing
     */
    auto add_lexical_rules() -> void;

    /**
     * Add all productions needed for json parsing
     */
    auto add_productions() -> void;

    /**
     * A semantic rule that needs access to m_bad_key_counter
     * @param m
     * @return std::unique_ptr<ParserAST>
     */
    auto bad_object_rule(NonTerminal* m) -> std::unique_ptr<ParserAST>;

    uint32_t m_bad_key_counter{0};
};

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
        // TODO: this is a hacky fix for now to deal with dictionaries turned into
        // strings
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

auto JsonLikeParser::bad_object_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
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

JsonLikeParser::JsonLikeParser() {
    init();
}

void JsonLikeParser::clear_child() {
    m_bad_key_counter = 0;
}

void JsonLikeParser::add_lexical_rules() {
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

// " request and response, importance=high, this is some text, status=low,
// memory=10GB"
void JsonLikeParser::add_productions() {
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
            std::bind(&JsonLikeParser::bad_object_rule, this, std::placeholders::_1)
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

/// TODO: json_like_string ending in comma is not handled
auto main() -> int {
    JsonLikeParser custom_parser;

    std::string json_like_string
            = "empty=,empty_dict = {}, some_text1 , a_random_key1=10, a_random_key2=true,"
              " some_text2, a_random_key3=some_value, some_text3, empty=, a_random_key4=123abc, "
              "a_random_key4==false =abc= ";
    auto ast1 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast1 = static_cast<JsonRecordAST*>(ast1.get());
    std::cout << "AST human readable output:" << json_record_ast1->print(true) << std::endl
              << std::endl;

    json_like_string = "Request and Response Information, SOME_REDUCED_PAYLOAD=null,"
                       " someId=0e820f76-104d-4b1d-b93a-fc1837a63efa, duration=21, bool=true, "
                       "almost-bool2=truefalse, almost-bool2=truer, "
                       "fakeRespHeaders=FA_KE_ID=0:FAKE_LOCALE_ID=en_US:x-o-fake-id=0:FA_KER_ID=0, "
                       "equal==123, equalint=123=123, equalbool=true=false";
    auto ast2 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast2 = static_cast<JsonRecordAST*>(ast2.get());
    std::cout << "AST human readable output:" << json_record_ast2->print(true) << std::endl
              << std::endl;

    json_like_string = "level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
                       "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
                       "a\\\"}";

    auto ast3 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast3 = static_cast<JsonRecordAST*>(ast3.get());
    std::cout << "AST human readable output:" << json_record_ast3->print(true) << std::endl
              << std::endl;
    /*
    json_like_string
            = "LogFuncArg(level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
              "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
              "a\\\"})";
    auto ast4 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast4 = static_cast<JsonRecordAST*>(ast4.get());
    std::cout << "AST human readable output:" << json_record_ast4->print(true) << std::endl
              << std::endl;
    */
    json_like_string = "log=asd{a=1, b=2}asd";
    auto ast5 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast5 = static_cast<JsonRecordAST*>(ast5.get());
    std::cout << "AST human readable output:" << json_record_ast5->print(true) << std::endl
              << std::endl;

    /*
    json_like_string = " Key = { } ,, comma_dict={,}, log={ test =123 , text =  },,,=, = ,
    log2={,,,}, log3={ , , ,  }, " "log4={{{,{,}}{}},,}}} "; auto ast6 =
    custom_parser.parse_input(json_like_string); auto* json_record_ast6 =
    static_cast<JsonRecordAST*>(ast6.get()); std::cout << "AST human readable output:" <<
    json_record_ast6->print(true) << std::endl
              << std::endl;

    json_like_string = "";
    auto ast7 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast7 = static_cast<JsonRecordAST*>(ast7.get());
    std::cout << "AST human readable output:" << json_record_ast7->print(true) << std::endl
              << std::endl;
    */
    return 0;
}
