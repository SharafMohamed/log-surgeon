#include <iostream>
#include <map>
#include <string>

#include <log_surgeon/Lexer.hpp>
#include <log_surgeon/Schema.hpp>

using log_surgeon::finite_automata::ByteDfaState;
using log_surgeon::finite_automata::ByteNfaState;
using log_surgeon::finite_automata::Dfa;
using log_surgeon::finite_automata::Nfa;
using log_surgeon::lexers::ByteLexer;
using log_surgeon::LexicalRule;
using log_surgeon::ParserAST;
using log_surgeon::SchemaVarAST;
using std::string;
using std::unique_ptr;
using std::vector;

using ByteLexicalRule = log_surgeon::LexicalRule<ByteNfaState>;

auto get_intersect_for_query(
        std::map<uint32_t, std::string>& m_id_symbol,
        Dfa<ByteDfaState> const& dfa1,
        std::string const& search_string
) -> void {
    std::string processed_search_string;
    // Replace all * with .*
    for (char const& c : search_string) {
        if (c == '*') {
            processed_search_string.push_back('.');
        }
        processed_search_string.push_back(c);
    }
    log_surgeon::Schema schema;
    schema.append_var(string("search:") + processed_search_string);
    auto schema_ast = schema.release_schema_ast_ptr();
    vector<ByteLexicalRule> rules;
    for (unique_ptr<ParserAST> const& parser_ast : schema_ast->m_schema_vars) {
        auto* schema_var_ast = dynamic_cast<SchemaVarAST*>(parser_ast.get());
        rules.emplace_back(0, std::move(schema_var_ast->m_regex_ptr));
    }
    Nfa<ByteNfaState> nfa(std::move(rules));
    Dfa<ByteDfaState> dfa2(std::move(nfa));
    auto schema_types = dfa1.get_intersect(&dfa2);
    std::cout << search_string << ":";
    for (auto const& schema_type : schema_types) {
        std::cout << m_id_symbol[schema_type] << ",";
    }
    std::cout << std::endl;
}

auto main() -> int {
    for (int i = 0; i < 2; i++) {
        log_surgeon::Schema schema;
        if (0 == i) {
            std::cout << "--Schema1--" << std::endl;
            schema.append_var("int:\\-{0,1}[0-9]+");
            schema.append_var("float:\\-{0,1}[0-9]+\\.[0-9]+");
            schema.append_var("hex:[a-fA-F]+");
            schema.append_var("hasNumber:.*\\d.*");
            schema.append_var("equals:.*=.*[a-zA-Z0-9].*");
            schema.append_var("logLevel:(INFO)|(DEBUG)|(WARN)|(ERROR)|(TRACE)|(FATAL)");
        } else {
            std::cout << "--Schema2--" << std::endl;
            schema.append_var("v1:1");
            schema.append_var("v2:2");
            schema.append_var("v3:3");
            schema.append_var("v4:abc12");
            schema.append_var("v5:23def");
            schema.append_var("v6:123");
        }
        std::map<uint32_t, std::string> m_id_symbol;
        auto schema_ast = schema.release_schema_ast_ptr();
        vector<ByteLexicalRule> rules;
        for (unique_ptr<ParserAST> const& parser_ast : schema_ast->m_schema_vars) {
            auto* var_ast = dynamic_cast<SchemaVarAST*>(parser_ast.get());
            rules.emplace_back(m_id_symbol.size(), std::move(var_ast->m_regex_ptr));
            m_id_symbol[m_id_symbol.size()] = var_ast->m_name;
        }
        Nfa<ByteNfaState> nfa(std::move(rules));
        Dfa<ByteDfaState> dfa(std::move(nfa));
        get_intersect_for_query(m_id_symbol, dfa, "*1*");
        get_intersect_for_query(m_id_symbol, dfa, "*a*");
        get_intersect_for_query(m_id_symbol, dfa, "*a1*");
        get_intersect_for_query(m_id_symbol, dfa, "*=*");
        get_intersect_for_query(m_id_symbol, dfa, "abc123");
        get_intersect_for_query(m_id_symbol, dfa, "=");
        get_intersect_for_query(m_id_symbol, dfa, "1");
        get_intersect_for_query(m_id_symbol, dfa, "a*1");
        get_intersect_for_query(m_id_symbol, dfa, "a1");
    }
    return 0;
}
