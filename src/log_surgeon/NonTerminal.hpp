#ifndef LOG_SURGEON_NONTERMINAL_HPP
#define LOG_SURGEON_NONTERMINAL_HPP

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <log_surgeon/ParserAst.hpp>
#include <log_surgeon/Production.hpp>
#include <log_surgeon/Token.hpp>
#include <log_surgeon/types.hpp>

namespace log_surgeon {

class NonTerminal {
public:
    NonTerminal() : m_children_start(0), m_production(nullptr), m_ast(nullptr) {}

    explicit NonTerminal(Production*);

    /**
     * Return the ith child's (body of production) MatchedSymbol as a Token.
     * Note: only children are needed (and stored) for performing semantic
     * actions (for the AST)
     * @param i
     * @return Token*
     */
    [[nodiscard]] auto token_cast(uint32_t i) const -> Token* {
        return &std::get<Token>(m_all_children[m_children_start + i]);
    }

    /**
     * Return the ith child's (body of production) MatchedSymbol as a
     * NonTerminal. Note: only children are needed (and stored) for performing
     * semantic actions (for the AST)
     * @param i
     * @return NonTerminal*
     */
    [[nodiscard]] auto non_terminal_cast(uint32_t i) const -> NonTerminal* {
        return &std::get<NonTerminal>(m_all_children[m_children_start + i]);
    }

    /**
     * Return the AST that relates this non_terminal's children together (based
     * on the production/syntax-rule that was determined to have generated them)
     * @return std::unique_ptr<ParserAST>
     */
    auto get_ast() -> std::unique_ptr<ParserAST>& { return m_ast; }

    template <typename T>
    auto cast_ast() -> T {
        auto* casted_value{dynamic_cast<T>(m_ast.get())};
        if (nullptr == casted_value) {
            throw std::invalid_argument(
                    "Failed to cast `" + std::string(typeid(m_ast).name()) + "` to `"
                    + std::string(typeid(T).name()) + "`."
            );
        }
        return casted_value;
    }

    template <typename T>
    auto release_ast() -> std::unique_ptr<T> {
        auto* casted_value{dynamic_cast<T*>(m_ast.release())};
        if (nullptr == casted_value) {
            throw std::invalid_argument(
                    "Failed to cast `" + std::string(typeid(m_ast).name()) + "` to `"
                    + std::string(typeid(T).name()) + "`."
            );
        }
        return std::unique_ptr<T>(casted_value);
    }

    static MatchedSymbol m_all_children[];
    static uint32_t m_next_children_start;
    uint32_t m_children_start;
    Production* m_production;
    std::unique_ptr<ParserAST> m_ast;
};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_NONTERMINAL_HPP
