#include "SchemaParser.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <fmt/core.h>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/finite_automata/Capture.hpp>
#include <log_surgeon/finite_automata/NfaState.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>
#include <log_surgeon/parser_types.hpp>
#include <log_surgeon/ParserAst.hpp>
#include <log_surgeon/Reader.hpp>

template <template <typename> class ast>
using RegexASTByteBase = ast<log_surgeon::finite_automata::ByteNfaState>;

using RegexASTByte = RegexASTByteBase<log_surgeon::finite_automata::RegexAST>;
using RegexASTGroupByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTGroup>;
using RegexASTIntegerByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTInteger>;
using RegexASTLiteralByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTLiteral>;
using RegexASTMultiplicationByte
        = RegexASTByteBase<log_surgeon::finite_automata::RegexASTMultiplication>;
using RegexASTOrByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTOr>;
using RegexASTCatByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTCat>;
using RegexASTCaptureByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTCapture>;
using RegexASTEmptyByte = RegexASTByteBase<log_surgeon::finite_automata::RegexASTEmpty>;

using ParserValueRegex = log_surgeon::ParserValue<std::unique_ptr<RegexASTByte>>;

using std::make_unique;
using std::string;
using std::string_view;
using std::unique_ptr;

namespace log_surgeon {
namespace {
template <typename T>
auto safely_cast_regex_ast(RegexASTByte* base_clase) -> T {
    auto* derived_class{dynamic_cast<T>(base_clase)};
    if (nullptr == derived_class) {
        throw std::invalid_argument(
                "Failed to cast `RegexASTByte*` to `" + std::string(typeid(T).name()) + "`."
        );
    }
    return derived_class;
}

template <typename T>
auto safely_cast_parser_ast(ParserAST* base_class) -> T {
    auto* derived_class{dynamic_cast<T>(base_class)};
    if (nullptr == derived_class) {
        throw std::invalid_argument(
                "Failed to cast `RegexASTByte*` to `" + std::string(typeid(T).name()) + "`."
        );
    }
    return derived_class;
}

auto new_identifier_rule(NonTerminal const* m) -> unique_ptr<IdentifierAST> {
    auto* alpha_numeric_token{m->token_cast(0)};
    auto const alpha_numberic_value{alpha_numeric_token->to_string()[0]};
    return make_unique<IdentifierAST>(IdentifierAST(alpha_numberic_value));
}

auto existing_identifier_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& identifier_token{m->non_terminal_cast(0)};
    auto* alpha_numeric_token{m->token_cast(1)};
    auto const alpha_numberic_value{alpha_numeric_token->to_string()[0]};
    identifier_token->cast_ast<IdentifierAST*>()->add_character(alpha_numberic_value);
    return std::move(identifier_token->get_ast());
}

auto schema_var_rule(NonTerminal const* m) -> unique_ptr<SchemaVarAST> {
    auto* identifier{m->non_terminal_cast(1)->cast_ast<IdentifierAST*>()};
    auto* colon_token{m->token_cast(2)};
    auto& regex{m->non_terminal_cast(3)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    return make_unique<SchemaVarAST>(identifier->m_name, std::move(regex), colon_token->m_line);
}

auto new_schema_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<SchemaAST> {
    return make_unique<SchemaAST>();
}

auto new_schema_rule_with_var(NonTerminal const* m) -> unique_ptr<SchemaAST> {
    auto schema_var{m->non_terminal_cast(0)->release_ast<SchemaVarAST>()};
    return make_unique<SchemaAST>(std::move(schema_var));
}

auto new_schema_rule_with_delimiters(NonTerminal const* m) -> unique_ptr<SchemaAST> {
    auto delimiters_string{m->non_terminal_cast(2)->release_ast<DelimiterStringAST>()};
    return make_unique<SchemaAST>(std::move(delimiters_string));
}

auto existing_schema_rule_with_delimiter(NonTerminal const* m) -> unique_ptr<SchemaAST> {
    auto schema_ast{m->non_terminal_cast(0)->release_ast<SchemaAST>()};
    auto& delimiters_string{m->non_terminal_cast(4)->get_ast()};
    schema_ast->add_delimiters(std::move(delimiters_string));
    return schema_ast;
}

auto existing_schema_rule(NonTerminal const* m) -> unique_ptr<SchemaAST> {
    auto schema_ast{m->non_terminal_cast(0)->release_ast<SchemaAST>()};
    auto& schema_var{m->non_terminal_cast(2)->get_ast()};
    schema_ast->append_schema_var(std::move(schema_var));
    // Can reset the buffers at this point
    if (NonTerminal::m_next_children_start > cSizeOfAllChildren / 2) {
        NonTerminal::m_next_children_start = 0;
    }
    return schema_ast;
}

auto regex_capture_rule(NonTerminal const* m) -> std::unique_ptr<ParserAST> {
    auto const* identifier{m->non_terminal_cast(3)->cast_ast<IdentifierAST*>()};
    auto& regex{m->non_terminal_cast(5)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    return make_unique<ParserValueRegex>(make_unique<RegexASTCaptureByte>(
            std::move(regex),
            make_unique<finite_automata::Capture>(identifier->m_name)
    ));
}

auto identity_rule_parser_ast_schema(NonTerminal const* m) -> unique_ptr<SchemaAST> {
    return m->non_terminal_cast(0)->release_ast<SchemaAST>();
}

auto regex_identity_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    return make_unique<ParserValueRegex>(
            std::move(m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>())
    );
}

auto regex_cat_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& lhs{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto& rhs{m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    return make_unique<ParserValueRegex>(
            make_unique<RegexASTCatByte>(std::move(lhs), std::move(rhs))
    );
}

auto regex_or_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& lhs{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto& rhs{m->non_terminal_cast(2)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    return make_unique<ParserValueRegex>(make_unique<RegexASTOrByte>(std::move(lhs), std::move(rhs))
    );
}

auto regex_match_zero_or_more_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& regex_group{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};

    // To handle negative captures we treat `R*` as `R+ | ∅`.
    return make_unique<ParserValueRegex>(make_unique<RegexASTOrByte>(
            make_unique<RegexASTEmptyByte>(),
            make_unique<RegexASTMultiplicationByte>(std::move(regex_group), 1, 0)
    ));
}

auto regex_match_one_or_more_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& regex_group{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    return make_unique<ParserValueRegex>(
            make_unique<RegexASTMultiplicationByte>(std::move(regex_group), 1, 0)
    );
}

auto regex_match_exactly_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& regex_group{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const& reps{
            m->non_terminal_cast(2)->get_ast()->get<unique_ptr<RegexASTIntegerByte>>()->get_value()
    };
    return make_unique<ParserValueRegex>(
            make_unique<RegexASTMultiplicationByte>(std::move(regex_group), reps, reps)
    );
}

auto regex_match_range_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& regex_group{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const& min_reps_ast{m->non_terminal_cast(2)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const min_reps{safely_cast_regex_ast<RegexASTIntegerByte*>(min_reps_ast.get())->get_value()
    };

    auto const& max_reps_ast{m->non_terminal_cast(4)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const max_reps{safely_cast_regex_ast<RegexASTIntegerByte*>(max_reps_ast.get())->get_value()
    };

    if (0 == min_reps) {
        // To handle negative captures we treat `R*` as `R+ | ∅`.
        return make_unique<ParserValueRegex>(make_unique<RegexASTOrByte>(
                make_unique<RegexASTEmptyByte>(),
                make_unique<RegexASTMultiplicationByte>(std::move(regex_group), 1, max_reps)
        ));
    }

    return make_unique<ParserValueRegex>(
            make_unique<RegexASTMultiplicationByte>(std::move(regex_group), min_reps, max_reps)
    );
}

auto regex_add_literal_existing_group_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast1{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* group_ast{safely_cast_regex_ast<RegexASTGroupByte*>(regex_ast1.get())};

    auto const& regex_ast2{m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* literal_ast{safely_cast_regex_ast<RegexASTLiteralByte*>(regex_ast2.get())};

    return make_unique<ParserValueRegex>(make_unique<RegexASTGroupByte>(group_ast, literal_ast));
}

auto regex_add_range_existing_group_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast1{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* group_ast1{safely_cast_regex_ast<RegexASTGroupByte*>(regex_ast1.get())};

    auto const& regex_ast2{m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* group_ast2{safely_cast_regex_ast<RegexASTGroupByte*>(regex_ast2.get())};

    return make_unique<ParserValueRegex>(make_unique<RegexASTGroupByte>(group_ast1, group_ast2));
}

auto regex_add_literal_new_group_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast{m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* literal_ast{safely_cast_regex_ast<RegexASTLiteralByte*>(regex_ast.get())};
    return make_unique<ParserValueRegex>(make_unique<RegexASTGroupByte>(literal_ast));
}

auto regex_add_range_new_group_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast{m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* group_ast{safely_cast_regex_ast<RegexASTGroupByte*>(regex_ast.get())};
    return make_unique<ParserValueRegex>(make_unique<RegexASTGroupByte>(group_ast));
}

auto regex_complement_incomplete_group_rule([[maybe_unused]] NonTerminal const* m
) -> unique_ptr<ParserAST> {
    return make_unique<ParserValueRegex>(make_unique<RegexASTGroupByte>());
}

auto regex_range_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast1{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const& regex_ast2{m->non_terminal_cast(2)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* begin_literal_ast{safely_cast_regex_ast<RegexASTLiteralByte*>(regex_ast1.get())};
    auto* end_literal_ast{safely_cast_regex_ast<RegexASTLiteralByte*>(regex_ast2.get())};
    return make_unique<ParserValueRegex>(
            make_unique<RegexASTGroupByte>(begin_literal_ast, end_literal_ast)
    );
}

auto regex_middle_identity_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    return make_unique<ParserValueRegex>(
            std::move(m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>())
    );
}

auto regex_literal_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const literal{m->token_cast(0)->to_string()[0]};
    return make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>(literal));
}

auto regex_cancel_literal_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const literal{m->token_cast(1)->to_string()[0]};
    return make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>(literal));
}

auto regex_existing_integer_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto* integer_ast{safely_cast_regex_ast<RegexASTIntegerByte*>(regex_ast.get())};
    auto const digit{m->token_cast(1)->to_string()[0]};
    return make_unique<ParserValueRegex>(make_unique<RegexASTIntegerByte>(integer_ast, digit));
}

auto regex_new_integer_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const digit{m->token_cast(0)->to_string()[0]};
    return make_unique<ParserValueRegex>(make_unique<RegexASTIntegerByte>(digit));
}

auto regex_digit_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return make_unique<ParserValueRegex>(make_unique<RegexASTGroupByte>('0', '9'));
}

auto regex_wildcard_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto regex_wildcard{make_unique<RegexASTGroupByte>(0, cUnicodeMax)};
    regex_wildcard->set_is_wildcard_true();
    return std::make_unique<ParserValueRegex>(std::move(regex_wildcard));
}

auto regex_vertical_tab_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return std::make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>('\v'));
}

auto regex_form_feed_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return std::make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>('\f'));
}

auto regex_tab_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return std::make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>('\t'));
}

auto regex_char_return_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return std::make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>('\r'));
}

auto regex_newline_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return std::make_unique<ParserValueRegex>(make_unique<RegexASTLiteralByte>('\n'));
}

auto regex_white_space_rule([[maybe_unused]] NonTerminal const* m) -> unique_ptr<ParserAST> {
    return std::make_unique<ParserValueRegex>(
            make_unique<RegexASTGroupByte>(RegexASTGroupByte({' ', '\t', '\r', '\n', '\v', '\f'}))
    );
}

auto existing_delimiter_string_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto& first_symbol{m->non_terminal_cast(0)->get_ast()};
    auto* delimiter_string_ast{safely_cast_parser_ast<DelimiterStringAST*>(first_symbol.get())};

    auto const& regex_ast{m->non_terminal_cast(1)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const character{
            safely_cast_regex_ast<RegexASTLiteralByte*>(regex_ast.get())->get_character()
    };

    delimiter_string_ast->add_delimiter(character);
    return std::move(first_symbol);
}

auto new_delimiter_string_rule(NonTerminal const* m) -> unique_ptr<ParserAST> {
    auto const& regex_ast{m->non_terminal_cast(0)->get_ast()->get<unique_ptr<RegexASTByte>>()};
    auto const character{
            safely_cast_regex_ast<RegexASTLiteralByte*>(regex_ast.get())->get_character()
    };
    return make_unique<DelimiterStringAST>(character);
}
}  // namespace

SchemaParser::SchemaParser() {
    add_lexical_rules();
    add_productions();
    generate();
}

auto SchemaParser::generate_schema_ast(Reader& reader) -> unique_ptr<SchemaAST> {
    auto nonterminal{parse(reader)};
    std::unique_ptr<SchemaAST> schema_ast(
            safely_cast_parser_ast<SchemaAST*>(nonterminal.get_ast().release())
    );
    return schema_ast;
}

auto SchemaParser::try_schema_file(string const& schema_file_path) -> unique_ptr<SchemaAST> {
    FileReader schema_reader;
    ErrorCode const error_code{schema_reader.try_open(schema_file_path)};
    if (ErrorCode::Success != error_code) {
        if (ErrorCode::Errno == error_code) {
            throw std::runtime_error(
                    fmt::format("Failed to read '{}', errno={}", schema_file_path, errno)
            );
        }
        throw std::runtime_error(
                fmt::format("Failed to read '{}', error_code={}", schema_file_path, error_code)
        );
    }
    SchemaParser sp;
    Reader reader{[&](char* buf, size_t count, size_t& read_to) -> ErrorCode {
        schema_reader.read(buf, count, read_to);
        if (read_to == 0) {
            return ErrorCode::EndOfFile;
        }
        return ErrorCode::Success;
    }};
    auto schema_ast{sp.generate_schema_ast(reader)};
    schema_reader.close();
    return schema_ast;
}

auto SchemaParser::try_schema_string(string_view const schema_string) -> unique_ptr<SchemaAST> {
    Reader reader{[&](char* dst_buf, size_t count, size_t& read_to) -> ErrorCode {
        uint32_t unparsed_string_pos{0};
        std::span<char> const buf{dst_buf, count};
        if (unparsed_string_pos + count > schema_string.length()) {
            count = schema_string.length() - unparsed_string_pos;
        }
        read_to = count;
        if (read_to == 0) {
            return ErrorCode::EndOfFile;
        }
        for (uint32_t i{0}; i < count; i++) {
            buf[i] = schema_string[unparsed_string_pos + i];
        }
        unparsed_string_pos += count;
        return ErrorCode::Success;
    }};
    SchemaParser sp;
    return sp.generate_schema_ast(reader);
}

auto SchemaParser::add_lexical_rules() -> void {
    if (m_special_regex_characters.empty()) {
        m_special_regex_characters.emplace('(', "Lparen");
        m_special_regex_characters.emplace(')', "Rparen");
        m_special_regex_characters.emplace('*', "Star");
        m_special_regex_characters.emplace('+', "Plus");
        m_special_regex_characters.emplace('-', "Dash");
        m_special_regex_characters.emplace('.', "Dot");
        m_special_regex_characters.emplace('[', "Lbracket");
        m_special_regex_characters.emplace(']', "Rbracket");
        m_special_regex_characters.emplace('\\', "Backslash");
        m_special_regex_characters.emplace('^', "Hat");
        m_special_regex_characters.emplace('{', "Lbrace");
        m_special_regex_characters.emplace('}', "Rbrace");
        m_special_regex_characters.emplace('|', "Vbar");
        m_special_regex_characters.emplace('<', "Langle");
        m_special_regex_characters.emplace('>', "Rangle");
        m_special_regex_characters.emplace('?', "QuestionMark");
    }
    for (auto const& [special_regex_char, special_regex_name] : m_special_regex_characters) {
        add_token(special_regex_name, special_regex_char);
    }
    add_token("Tab", '\t');
    add_token("NewLine", '\n');
    add_token("VerticalTab", '\v');
    add_token("FormFeed", '\f');
    add_token("CarriageReturn", '\r');
    add_token("Space", ' ');
    add_token("Bang", '!');
    add_token("Quotation", '"');
    add_token("Hash", '#');
    add_token("DollarSign", '$');
    add_token("Percent", '%');
    add_token("Ampersand", '&');
    add_token("Apostrophe", '\'');
    add_token("Comma", ',');
    add_token("ForwardSlash", '/');
    add_token_group("Numeric", make_unique<RegexASTGroupByte>('0', '9'));
    add_token("Colon", ':');
    add_token("SemiColon", ';');
    add_token("Equal", '=');
    add_token("At", '@');
    add_token_group("AlphaNumeric", make_unique<RegexASTGroupByte>('a', 'z'));
    add_token_group("AlphaNumeric", make_unique<RegexASTGroupByte>('A', 'Z'));
    add_token_group("AlphaNumeric", make_unique<RegexASTGroupByte>('0', '9'));
    add_token("Underscore", '_');
    add_token("Backtick", '`');
    add_token("Tilde", '~');
    add_token("d", 'd');
    add_token("s", 's');
    add_token("n", 'n');
    add_token("r", 'r');
    add_token("t", 't');
    add_token("f", 'f');
    add_token("v", 'v');
    add_token_chain("Delimiters", "delimiters");
    // `RegexASTGroupByte` default constructs to an `m_negate` group, so we add the only two
    // characters which can't be in a comment, the newline and carriage return characters as they
    // signify the end of the comment.
    auto comment_characters{make_unique<RegexASTGroupByte>()};
    comment_characters->add_literal('\r');
    comment_characters->add_literal('\n');
    add_token_group("CommentCharacters", std::move(comment_characters));
}

auto SchemaParser::add_productions() -> void {
    // add_production("Schema", {}, new_schema_rule);
    add_production("Schema", {"Comment"}, new_schema_rule);
    add_production("Schema", {"SchemaVar"}, new_schema_rule_with_var);
    add_production(
            "Schema",
            {"Delimiters", "Colon", "DelimiterString"},
            new_schema_rule_with_delimiters
    );
    add_production("Schema", {"Schema", "PortableNewLine"}, identity_rule_parser_ast_schema);
    add_production(
            "Schema",
            {"Schema", "PortableNewLine", "Comment"},
            identity_rule_parser_ast_schema
    );
    add_production("Schema", {"Schema", "PortableNewLine", "SchemaVar"}, existing_schema_rule);
    add_production(
            "Schema",
            {"Schema", "PortableNewLine", "Delimiters", "Colon", "DelimiterString"},
            existing_schema_rule_with_delimiter
    );
    add_production(
            "DelimiterString",
            {"DelimiterString", "Literal"},
            existing_delimiter_string_rule
    );
    add_production("DelimiterString", {"Literal"}, new_delimiter_string_rule);
    add_production("PortableNewLine", {"CarriageReturn", "NewLine"}, nullptr);
    add_production("PortableNewLine", {"NewLine"}, nullptr);
    add_production("Comment", {"ForwardSlash", "ForwardSlash", "Text"}, nullptr);
    add_production("Text", {"Text", "CommentCharacters"}, nullptr);
    add_production("Text", {"CommentCharacters"}, nullptr);
    add_production("Text", {"Text", "Delimiters"}, nullptr);
    add_production("Text", {"Delimiters"}, nullptr);
    add_production(
            "SchemaVar",
            {"WhitespaceStar", "Identifier", "Colon", "Regex"},
            schema_var_rule
    );
    add_production("Identifier", {"Identifier", "AlphaNumeric"}, existing_identifier_rule);
    add_production("Identifier", {"AlphaNumeric"}, new_identifier_rule);
    add_production("WhitespaceStar", {"WhitespaceStar", "Space"}, nullptr);
    add_production("WhitespaceStar", {}, nullptr);
    add_production("Regex", {"Concat"}, regex_identity_rule);
    add_production("Concat", {"Concat", "Or"}, regex_cat_rule);
    add_production("Concat", {"Or"}, regex_identity_rule);
    add_production("Or", {"Or", "Vbar", "Literal"}, regex_or_rule);
    add_production("Or", {"MatchStar"}, regex_identity_rule);
    add_production("Or", {"MatchPlus"}, regex_identity_rule);
    add_production("Or", {"MatchExact"}, regex_identity_rule);
    add_production("Or", {"MatchRange"}, regex_identity_rule);
    add_production("Or", {"CompleteGroup"}, regex_identity_rule);
    add_production("MatchStar", {"CompleteGroup", "Star"}, regex_match_zero_or_more_rule);
    add_production("MatchPlus", {"CompleteGroup", "Plus"}, regex_match_one_or_more_rule);
    add_production(
            "MatchExact",
            {"CompleteGroup", "Lbrace", "Integer", "Rbrace"},
            regex_match_exactly_rule
    );
    add_production(
            "MatchRange",
            {"CompleteGroup", "Lbrace", "Integer", "Comma", "Integer", "Rbrace"},
            regex_match_range_rule
    );
    add_production("CompleteGroup", {"IncompleteGroup", "Rbracket"}, regex_identity_rule);
    add_production("CompleteGroup", {"Literal"}, regex_identity_rule);
    add_production("CompleteGroup", {"Digit"}, regex_identity_rule);
    add_production("CompleteGroup", {"Wildcard"}, regex_identity_rule);
    add_production("CompleteGroup", {"WhiteSpace"}, regex_identity_rule);
    add_production(
            "IncompleteGroup",
            {"IncompleteGroup", "LiteralRange"},
            regex_add_range_existing_group_rule
    );
    add_production(
            "IncompleteGroup",
            {"IncompleteGroup", "Digit"},
            regex_add_range_existing_group_rule
    );
    add_production(
            "IncompleteGroup",
            {"IncompleteGroup", "Literal"},
            regex_add_literal_existing_group_rule
    );
    add_production(
            "IncompleteGroup",
            {"IncompleteGroup", "WhiteSpace"},
            regex_add_literal_existing_group_rule
    );
    add_production("IncompleteGroup", {"Lbracket", "LiteralRange"}, regex_add_range_new_group_rule);
    add_production("IncompleteGroup", {"Lbracket", "Digit"}, regex_add_range_new_group_rule);
    add_production("IncompleteGroup", {"Lbracket", "Literal"}, regex_add_literal_new_group_rule);
    add_production("IncompleteGroup", {"Lbracket", "WhiteSpace"}, regex_add_literal_new_group_rule);
    add_production("IncompleteGroup", {"Lbracket", "Hat"}, regex_complement_incomplete_group_rule);
    add_production("LiteralRange", {"Literal", "Dash", "Literal"}, regex_range_rule);
    add_production("Literal", {"Backslash", "t"}, regex_tab_rule);
    add_production("Literal", {"Backslash", "n"}, regex_newline_rule);
    add_production("Literal", {"Backslash", "v"}, regex_vertical_tab_rule);
    add_production("Literal", {"Backslash", "f"}, regex_form_feed_rule);
    add_production("Literal", {"Backslash", "r"}, regex_char_return_rule);
    add_production("Literal", {"Space"}, regex_literal_rule);
    add_production("Literal", {"Bang"}, regex_literal_rule);
    add_production("Literal", {"Quotation"}, regex_literal_rule);
    add_production("Literal", {"Hash"}, regex_literal_rule);
    add_production("Literal", {"DollarSign"}, regex_literal_rule);
    add_production("Literal", {"Percent"}, regex_literal_rule);
    add_production("Literal", {"Ampersand"}, regex_literal_rule);
    add_production("Literal", {"Apostrophe"}, regex_literal_rule);
    add_production("Literal", {"Backslash", "Lparen"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Rparen"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Star"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Plus"}, regex_cancel_literal_rule);
    add_production("Literal", {"Comma"}, regex_literal_rule);
    add_production("Literal", {"Backslash", "Dash"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Dot"}, regex_cancel_literal_rule);
    add_production("Literal", {"ForwardSlash"}, regex_literal_rule);
    add_production("Literal", {"AlphaNumeric"}, regex_literal_rule);
    add_production("Literal", {"Colon"}, regex_literal_rule);
    add_production("Literal", {"SemiColon"}, regex_literal_rule);
    add_production("Literal", {"Equal"}, regex_literal_rule);
    add_production("Literal", {"At"}, regex_literal_rule);
    add_production("Literal", {"Backslash", "Lbracket"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Backslash"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Rbracket"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Hat"}, regex_cancel_literal_rule);
    add_production("Literal", {"Underscore"}, regex_literal_rule);
    add_production("Literal", {"Backtick"}, regex_literal_rule);
    add_production("Literal", {"Backslash", "Lbrace"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Vbar"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Rbrace"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Langle"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "Rangle"}, regex_cancel_literal_rule);
    add_production("Literal", {"Backslash", "QuestionMark"}, regex_cancel_literal_rule);
    add_production("Literal", {"Tilde"}, regex_literal_rule);
    add_production(
            "Literal",
            {"Lparen", "QuestionMark", "Langle", "Identifier", "Rangle", "Regex", "Rparen"},
            regex_capture_rule
    );
    add_production("Literal", {"Lparen", "Regex", "Rparen"}, regex_middle_identity_rule);
    for (auto const& [special_regex_char, special_regex_name] : m_special_regex_characters) {
        std::ignore = special_regex_char;
        add_production("Literal", {"Backslash", special_regex_name}, regex_cancel_literal_rule);
    }
    add_production("Integer", {"Integer", "Numeric"}, regex_existing_integer_rule);
    add_production("Integer", {"Numeric"}, regex_new_integer_rule);
    add_production("Digit", {"Backslash", "d"}, regex_digit_rule);
    add_production("Wildcard", {"Dot"}, regex_wildcard_rule);
    add_production("WhiteSpace", {"Backslash", "s"}, regex_white_space_rule);
}
}  // namespace log_surgeon
