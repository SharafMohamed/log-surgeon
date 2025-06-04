#include "LogParser.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/ParserAst.hpp>
#include <log_surgeon/SchemaParser.hpp>

using std::make_unique;
using std::runtime_error;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace log_surgeon {
using finite_automata::ByteDfaState;
using finite_automata::ByteNfaState;
using finite_automata::RegexAST;
using finite_automata::RegexASTCat;
using finite_automata::RegexASTGroup;
using finite_automata::RegexASTInteger;
using finite_automata::RegexASTLiteral;
using finite_automata::RegexASTMultiplication;
using finite_automata::RegexASTOr;

LogParser::LogParser(string const& schema_file_path)
        : LogParser::LogParser(SchemaParser::try_schema_file(schema_file_path)) {}

LogParser::LogParser(std::unique_ptr<SchemaAST> schema_ast) {
    add_rules(std::move(schema_ast));
    m_lexer.generate();
    m_log_event_view = make_unique<LogEventView>(*this);
}

auto LogParser::set_delimiters(unique_ptr<ParserAST> const& delimiters) -> void {
    auto* delimiters_ptr = dynamic_cast<DelimiterStringAST*>(delimiters.get());
    if (delimiters_ptr != nullptr) {
        m_lexer.set_delimiters(delimiters_ptr->m_delimiters);
    }
}

auto LogParser::add_rules(std::unique_ptr<SchemaAST> schema_ast) -> void {
    for (auto const& delimiters : schema_ast->m_delimiters) {
        set_delimiters(delimiters);
    }
    vector<uint32_t> delimiters;
    for (uint32_t i = 0; i < cSizeOfByte; i++) {
        if (m_lexer.is_delimiter(i)) {
            delimiters.push_back(i);
        }
    }

    // Required to have delimiters
    if (delimiters.empty()) {
        throw runtime_error("When using --schema-path, \"delimiters:\" line must be used.");
    }
    add_token("newLine", '\n');
    for (unique_ptr<ParserAST> const& parser_ast : schema_ast->m_schema_vars) {
        auto* rule = dynamic_cast<SchemaVarAST*>(parser_ast.get());
        rule->m_regex_ptr->remove_delimiters_from_wildcard(delimiters);
        std::array<bool, cSizeOfUnicode> is_possible_input{};
        rule->m_regex_ptr->set_possible_inputs_to_true(is_possible_input);

        if (rule->m_name == "header") {
            // Need to add a special character to specify this character in a schema file e.g. \B
            auto start_of_file_regex = make_unique<RegexASTMultiplication<ByteNfaState>>(
                    make_unique<RegexASTLiteral<ByteNfaState>>(utf8::cCharStartOfFile),
                    0,
                    1
            );
            rule->m_regex_ptr = make_unique<RegexASTCat<ByteNfaState>>(
                    std::move(start_of_file_regex),
                    std::move(rule->m_regex_ptr)
            );
        } else {
            // For log-specific lexing: modify variable regex to contain a delimiter at the start.
            auto delimiter_group = make_unique<RegexASTGroup<ByteNfaState>>(
                    RegexASTGroup<ByteNfaState>(delimiters)
            );
            rule->m_regex_ptr = make_unique<RegexASTCat<ByteNfaState>>(
                    std::move(delimiter_group),
                    std::move(rule->m_regex_ptr)
            );
        }

        add_rule(rule->m_name, std::move(rule->m_regex_ptr));
    }
}

auto LogParser::reset() -> void {
    m_input_buffer.reset();
    m_lexer.reset();
    m_lexer.prepend_start_of_file_char(m_input_buffer);
}

auto LogParser::parse_and_generate_metadata(LogParser::ParsingAction& parsing_action) -> ErrorCode {
    ErrorCode error_code = parse(parsing_action);
    if (ErrorCode::Success == error_code) {
        generate_log_event_view_metadata();
    }
    return error_code;
}

auto LogParser::parse(LogParser::ParsingAction& parsing_action) -> ErrorCode {
    std::unique_ptr<LogParserOutputBuffer>& output_buffer = m_log_event_view->m_log_output_buffer;
    if (0 == output_buffer->pos()) {
        output_buffer->set_has_delimiters(m_lexer.get_has_delimiters());
        Token next_token;
        if (m_has_start_of_log) {
            next_token = m_start_of_log_message;
        } else {
            auto [err, optional_next_token] = get_next_symbol();
            if (ErrorCode::Success != err) {
                return err;
            }
            next_token = optional_next_token.value();
            if (false == output_buffer->has_header()
                && next_token.m_type_ids_ptr->at(0) == (uint32_t)(SymbolId::TokenHeader))
            {
                // TODO: combine the below with found_start_of_next_message into 1 function
                // Increment by 1 because the '\n' character is not part of the next log message
                m_start_of_log_message = next_token;
                if (m_start_of_log_message.m_start_pos == m_start_of_log_message.m_buffer_size - 1)
                {
                    m_start_of_log_message.m_start_pos = 0;
                } else {
                    m_start_of_log_message.m_start_pos++;
                }
                // make a message with just the '\n' character
                next_token.m_end_pos = next_token.m_start_pos + 1;
                next_token.m_type_ids_ptr
                        = &Lexer<ByteNfaState, ByteDfaState>::cTokenUncaughtStringTypes;
                output_buffer->set_token(1, next_token);
                output_buffer->set_pos(2);
                m_input_buffer.set_consumed_pos(next_token.m_start_pos);
                m_has_start_of_log = true;
                parsing_action = ParsingAction::Compress;
                return ErrorCode::Success;
            }
        }
        if (next_token.m_type_ids_ptr->at(0) == (uint32_t)SymbolId::TokenEnd) {
            output_buffer->set_token(0, next_token);
            output_buffer->set_pos(1);
            parsing_action = ParsingAction::CompressAndFinish;
            return ErrorCode::Success;
        }
        if (next_token.m_type_ids_ptr->at(0) == (uint32_t)SymbolId::TokenHeader) {
            output_buffer->set_has_header(true);
            output_buffer->set_token(0, next_token);
            // TODO: this is a problem because if multiple headers are defined, they'll all think
            // they contain the same captures
            auto optional_capture_ids{m_lexer.get_capture_ids_from_rule_id(
                    static_cast<uint32_t>(SymbolId::TokenHeader)
            )};
            if (optional_capture_ids.has_value()) {
                for (auto const capture_id : optional_capture_ids.value()) {
                    if (m_lexer.m_id_symbol[capture_id].starts_with("timestamp")) {
                        auto [start_reg_id, end_reg_id]{
                                m_lexer.get_reg_ids_from_capture_id(capture_id).value()
                        };
                        auto start_pos{next_token.get_reversed_reg_positions(start_reg_id)};
                        auto end_pos{next_token.get_reversed_reg_positions(end_reg_id)};
                        auto timestamp{next_token.get_capture_string(start_pos[0], end_pos[0])};
                        output_buffer->set_timestamp(timestamp);
                    }
                }
            }

            output_buffer->set_pos(1);
        } else {
            output_buffer->set_has_header(false);
            output_buffer->set_timestamp("");
            output_buffer->set_token(1, next_token);
            output_buffer->set_pos(2);
        }
        m_has_start_of_log = false;
    }

    while (true) {
        auto [err, optional_next_token] = get_next_symbol();
        if (ErrorCode::Success != err) {
            return err;
        }
        Token next_token{optional_next_token.value()};
        output_buffer->set_curr_token(next_token);
        auto token_type = next_token.m_type_ids_ptr->at(0);
        bool found_start_of_next_message
                = (output_buffer->has_header() && token_type == (uint32_t)SymbolId::TokenHeader)
                  || (!output_buffer->has_header() && next_token.get_char(0) == '\n'
                      && token_type != (uint32_t)SymbolId::TokenNewline);
        if (token_type == (uint32_t)SymbolId::TokenEnd) {
            parsing_action = ParsingAction::CompressAndFinish;
            return ErrorCode::Success;
        }
        if (false == output_buffer->has_header() && token_type == (uint32_t)SymbolId::TokenNewline)
        {
            m_input_buffer.set_consumed_pos(output_buffer->get_curr_token().m_end_pos);
            output_buffer->advance_to_next_token();
            parsing_action = ParsingAction::Compress;
            return ErrorCode::Success;
        }
        if (found_start_of_next_message) {
            // increment by 1 because the '\n' character is not part of the next
            // log message
            m_start_of_log_message = output_buffer->get_curr_token();
            if (m_start_of_log_message.m_start_pos == m_start_of_log_message.m_buffer_size - 1) {
                m_start_of_log_message.m_start_pos = 0;
            } else {
                m_start_of_log_message.m_start_pos++;
            }
            // make the last token of the current message the '\n' character
            Token curr_token = output_buffer->get_curr_token();
            curr_token.m_end_pos = curr_token.m_start_pos + 1;
            curr_token.m_type_ids_ptr
                    = &Lexer<ByteNfaState, ByteDfaState>::cTokenUncaughtStringTypes;
            output_buffer->set_curr_token(curr_token);
            if (0 == m_start_of_log_message.m_start_pos) {
                m_input_buffer.set_consumed_pos(m_input_buffer.storage().size() - 1);
            } else {
                m_input_buffer.set_consumed_pos(m_start_of_log_message.m_start_pos - 1);
            }
            m_has_start_of_log = true;
            output_buffer->advance_to_next_token();
            parsing_action = ParsingAction::Compress;
            return ErrorCode::Success;
        }
        output_buffer->advance_to_next_token();
    }
}

auto LogParser::get_symbol_id(std::string const& symbol) const -> std::optional<uint32_t> {
    if (auto const& it{m_lexer.m_symbol_id.find(symbol)}; it != m_lexer.m_symbol_id.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto LogParser::get_next_symbol() -> std::pair<ErrorCode, std::optional<Token>> {
    return m_lexer.scan(m_input_buffer);
}

auto LogParser::generate_log_event_view_metadata() -> void {
    uint32_t start = 0;
    if (false == m_log_event_view->m_log_output_buffer->has_header()) {
        start = 1;
    }
    uint32_t first_newline_pos{0};
    for (uint32_t i = start; i < m_log_event_view->m_log_output_buffer->pos(); i++) {
        Token* token = &m_log_event_view->m_log_output_buffer->get_mutable_token(i);
        m_log_event_view->add_token(token->m_type_ids_ptr->at(0), token);
        if (token->get_delimiter() == "\n" && first_newline_pos == 0) {
            first_newline_pos = i;
        }
    }
    // To be a multiline log there must be at least one token between the newline token and the last
    // token in the output buffer.
    if (m_log_event_view->m_log_output_buffer->has_header() && 0 < first_newline_pos
        && first_newline_pos + 1 < m_log_event_view->m_log_output_buffer->pos())
    {
        m_log_event_view->set_multiline(true);
    }
}
}  // namespace log_surgeon
