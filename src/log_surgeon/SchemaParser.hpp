#ifndef LOG_SURGEON_SCHEMA_PARSER_HPP
#define LOG_SURGEON_SCHEMA_PARSER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <log_surgeon/finite_automata/DfaState.hpp>
#include <log_surgeon/finite_automata/NfaState.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>
#include <log_surgeon/Lalr1Parser.hpp>
#include <log_surgeon/ParserAst.hpp>
#include <log_surgeon/Reader.hpp>

namespace log_surgeon {
class DelimiterStringAST : public ParserAST {
public:
    // Constructor
    explicit DelimiterStringAST(uint32_t const delimiter) { m_delimiters.push_back(delimiter); }

    auto add_delimiter(uint32_t const delimiter) -> void { m_delimiters.push_back(delimiter); }

    std::vector<uint32_t> m_delimiters;
};

class SchemaVarAST : public ParserAST {
public:
    // Constructor
    SchemaVarAST(
            std::string name,
            std::unique_ptr<finite_automata::RegexAST<finite_automata::ByteNfaState>> regex_ptr,
            uint32_t line_num
    )
            : m_line_num(line_num),
              m_name(std::move(name)),
              m_regex_ptr(std::move(regex_ptr)) {}

    uint32_t m_line_num;
    std::string m_name;
    std::unique_ptr<finite_automata::RegexAST<finite_automata::ByteNfaState>> m_regex_ptr;
};

class SchemaAST : public ParserAST {
public:
    SchemaAST() = default;

    explicit SchemaAST(std::unique_ptr<DelimiterStringAST> schema_var) {
        add_delimiters(std::move(schema_var));
    }

    explicit SchemaAST(std::unique_ptr<SchemaVarAST> schema_var) {
        append_schema_var(std::move(schema_var));
    }

    auto add_delimiters(std::unique_ptr<ParserAST> delimiters_in) -> void {
        m_delimiters.push_back(std::move(delimiters_in));
    }

    auto append_schema_var(std::unique_ptr<ParserAST> schema_var) -> void {
        m_schema_vars.push_back(std::move(schema_var));
    }

    auto insert_schema_var(std::unique_ptr<ParserAST> schema_var, uint32_t const pos) -> void {
        m_schema_vars.insert(m_schema_vars.begin() + pos, std::move(schema_var));
    }

    std::vector<std::unique_ptr<ParserAST>> m_schema_vars;
    std::vector<std::unique_ptr<ParserAST>> m_delimiters;
    std::string m_file_path;
};

class IdentifierAST : public ParserAST {
public:
    IdentifierAST() = default;

    explicit IdentifierAST(char const character) { m_name.push_back(character); }

    auto add_character(char const character) -> void { m_name.push_back(character); }

    std::string m_name;
};

class SchemaParser
        : public Lalr1Parser<finite_automata::ByteNfaState, finite_automata::ByteDfaState> {
public:
    virtual ~SchemaParser() = default;

    SchemaParser(SchemaParser const& rhs) = delete;
    auto operator=(SchemaParser const& rhs) -> SchemaParser& = delete;
    SchemaParser(SchemaParser&& rhs) noexcept = default;
    auto operator=(SchemaParser&& rhs) noexcept -> SchemaParser& = default;

    /**
     * File wrapper around generate_schema_ast()
     * @param schema_file_path
     * @return std::unique_ptr<SchemaAST>
     */
    static auto try_schema_file(std::string const& schema_file_path) -> std::unique_ptr<SchemaAST>;

    /**
     * String wrapper around generate_schema_ast()
     * @param schema_string
     * @return std::unique_ptr<SchemaAST>
     */
    static auto try_schema_string(std::string_view schema_string) -> std::unique_ptr<SchemaAST>;

    static auto get_special_regex_characters() -> std::unordered_map<char, std::string> const& {
        return m_special_regex_characters;
    }

private:
    SchemaParser();

    /**
     * Add all lexical rules needed for schema lexing
     */
    auto add_lexical_rules() -> void;

    /**
     * Add all productions needed for schema parsing
     */
    auto add_productions() -> void;

    /**
     * Parse a user defined schema to generate a schema AST used for generating
     * the log lexer
     * @param reader
     * @return std::unique_ptr<SchemaAST>
     */
    auto generate_schema_ast(Reader& reader) -> std::unique_ptr<SchemaAST>;

    static inline std::unordered_map<char, std::string> m_special_regex_characters;
};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_SCHEMA_PARSER_HPP
