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
    JsonValueAST(std::string const& value, JsonValueType type);

    JsonValueAST(std::unique_ptr<ParserAST> json_object_ast);

    auto add_character(char character) -> void { m_value.push_back(character); }

    auto add_string(std::string const& str) -> void { m_value += str; }

    auto add_dictionary_object(std::unique_ptr<ParserAST> json_object_ast) -> void {
        m_dictonary_json_objects.push_back(std::move(json_object_ast));
    }

    auto change_type(JsonValueType type) -> void { m_type = type; }

    auto get_value() const -> std::string const& { return m_value; }

    auto print(bool with_types) -> std::string;

    std::string m_value;
    JsonValueType m_type;
    std::vector<std::unique_ptr<ParserAST>> m_dictonary_json_objects;
};

class JsonObjectAST : public ParserAST {
public:
    // Constructor
    JsonObjectAST(std::string const& key, std::unique_ptr<ParserAST>& value_ast)
            : m_key(key),
              m_value_ast(std::move(value_ast)) {}

    JsonObjectAST(uint32_t& bad_key_counter, std::unique_ptr<ParserAST>& value_ast)
            : m_key("key" + std::to_string(bad_key_counter++)),
              m_value_ast(std::move(value_ast)) {}

    auto print(bool with_types) -> std::string {
        std::string output = "\"" + m_key + "\"";
        output += ":";
        auto* value_ptr = static_cast<JsonValueAST*>(m_value_ast.get());
        output += value_ptr->print(with_types);
        return output;
    }

    std::string m_key;
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
        std::string output = "\n";
        for (auto const& object_ast : m_object_asts) {
            auto* object_ptr = dynamic_cast<JsonObjectAST*>(object_ast.get());
            output += object_ptr->print(with_types);
            output += "\n";
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

    auto parse_json_like_string(std::string const& json_like_string)
            -> std::unique_ptr<JsonRecordAST>;

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
    auto bad_json_object_rule(NonTerminal* m) -> std::unique_ptr<ParserAST>;

    uint32_t m_bad_key_counter{0};
};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_CUSTOM_PARSER_HPP
