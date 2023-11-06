#ifndef LOG_SURGEON_CUSTOM_PARSER_HPP
#define LOG_SURGEON_CUSTOM_PARSER_HPP

#include <utility>

#include <log_surgeon/LALR1Parser.hpp>

namespace log_surgeon {

class JsonObjectAST;

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

class CustomParser : public LALR1Parser<
                             finite_automata::RegexNFAByteState,
                             finite_automata::RegexDFAByteState> {
public:
    // Constructor
    CustomParser();

    auto parse_input(std::string const& json_like_string) -> std::unique_ptr<ParserAST>;

private:
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
}  // namespace log_surgeon

#endif  // LOG_SURGEON_CUSTOM_PARSER_HPP
