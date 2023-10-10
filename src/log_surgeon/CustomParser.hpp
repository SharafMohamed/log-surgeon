#ifndef LOG_SURGEON_CUSTOM_PARSER_HPP
#define LOG_SURGEON_CUSTOM_PARSER_HPP

#include <utility>

#include <log_surgeon/LALR1Parser.hpp>

namespace log_surgeon {
enum class JsonValueType {
    Integer,
    Boolean,
    String,
    Dictionary,
    List
};

// ASTs used in CustomParser AST
class JsonValueAST : public ParserAST {
public:
    // Constructor
    JsonValueAST(std::string& value, JsonValueType type) : m_value(value), m_type(type) {}

    auto add_character(char character) -> void { m_value.push_back(character); }

    auto get_value() const -> std::string const& { return m_value; }

    auto print() -> std::string {
        if (m_type == JsonValueType::String) {
            return "\"" + m_value + "\"";
        }
        return m_value;
    }

    std::string m_value;
    JsonValueType m_type;
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

    auto print() -> std::string {
        std::string output = "\"" + m_key + "\"";
        output += ":";
        auto* value_ptr = dynamic_cast<JsonValueAST*>(m_value_ast.get());
        output += value_ptr->print();
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

    auto print() -> std::string {
        std::string output = "{";
        for (auto const& object_ast : m_object_asts) {
            auto* object_ptr = dynamic_cast<JsonObjectAST*>(object_ast.get());
            output += object_ptr->print();
            output += ",";
        }
        output.pop_back();
        output += "}";
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
    struct JsonNode {
        std::string key;
        std::vector<std::unique_ptr<JsonNode>> value;
    };

    JsonNode m_json_tree_root;

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
