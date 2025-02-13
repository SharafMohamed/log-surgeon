#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <log_surgeon/finite_automata/Nfa.hpp>
#include <log_surgeon/finite_automata/NfaState.hpp>
#include <log_surgeon/LexicalRule.hpp>
#include <log_surgeon/Schema.hpp>
#include <log_surgeon/SchemaParser.hpp>

using log_surgeon::finite_automata::ByteNfaState;
using log_surgeon::Schema;
using log_surgeon::SchemaVarAST;
using std::string;
using std::stringstream;
using std::vector;

using ByteLexicalRule = log_surgeon::LexicalRule<ByteNfaState>;
using ByteNfa = log_surgeon::finite_automata::Nfa<ByteNfaState>;

TEST_CASE("Test NFA", "[NFA]") {
    Schema schema;
    string const var_name{"capture"};
    string const var_schema{
            var_name + ":"
            + "Z|(A(?<letter>((?<letter1>(a)|(b))|(?<letter2>(c)|(d))))B(?"
              "<containerID>\\d+)C)"
    };
    schema.add_variable(var_schema, -1);

    auto const schema_ast = schema.release_schema_ast_ptr();
    auto& capture_rule_ast = dynamic_cast<SchemaVarAST&>(*schema_ast->m_schema_vars[0]);
    vector<ByteLexicalRule> rules;
    rules.emplace_back(0, std::move(capture_rule_ast.m_regex_ptr));
    ByteNfa const nfa{rules};

    // Compare against expected output
    // capture order(tags in brackets): letter1(0,1), letter2(2,3), letter(4,5), containerID(6,7)
    string expected_serialized_nfa = "0:byte_transitions={A-->1,Z-->2},"
                                     "spontaneous_transition={}\n";
    expected_serialized_nfa += "1:byte_transitions={},"
                               "spontaneous_transition={3[4p]}\n";
    expected_serialized_nfa += "2:byte_transitions={},"
                               "spontaneous_transition={4[0n,1n,2n,3n,4n,5n,6n,7n]}\n";
    expected_serialized_nfa += "3:byte_transitions={},"
                               "spontaneous_transition={5[0p],6[2p]}\n";
    expected_serialized_nfa += "4:accepting_tag=0,byte_transitions={},"
                               "spontaneous_transition={}\n";
    expected_serialized_nfa += "5:byte_transitions={a-->7,b-->7},"
                               "spontaneous_transition={}\n";
    expected_serialized_nfa += "6:byte_transitions={c-->8,d-->8},"
                               "spontaneous_transition={}\n";
    expected_serialized_nfa += "7:byte_transitions={},"
                               "spontaneous_transition={9[1p]}\n";
    expected_serialized_nfa += "8:byte_transitions={},"
                               "spontaneous_transition={10[3p]}\n";
    expected_serialized_nfa += "9:byte_transitions={},"
                               "spontaneous_transition={11[2n,3n]}\n";
    expected_serialized_nfa += "10:byte_transitions={},"
                               "spontaneous_transition={11[0n,1n]}\n";
    expected_serialized_nfa += "11:byte_transitions={},"
                               "spontaneous_transition={12[5p]}\n";
    expected_serialized_nfa += "12:byte_transitions={B-->13},"
                               "spontaneous_transition={}\n";
    expected_serialized_nfa += "13:byte_transitions={},"
                               "spontaneous_transition={14[6p]}\n";
    expected_serialized_nfa += "14:byte_transitions={0-->15,1-->15,2-->15,3-->15,4-->15,5-->15,6-->"
                               "15,7-->15,8-->15,9-->15},"
                               "spontaneous_transition={}\n";
    expected_serialized_nfa += "15:byte_transitions={0-->15,1-->15,2-->15,3-->15,4-->15,5-->15,6-->"
                               "15,7-->15,8-->15,9-->15},"
                               "spontaneous_transition={16[7p]}\n";
    expected_serialized_nfa += "16:byte_transitions={C-->4},"
                               "spontaneous_transition={}\n";

    // Compare expected and actual line-by-line
    auto const optional_actual_serialized_nfa = nfa.serialize();
    REQUIRE(optional_actual_serialized_nfa.has_value());
    stringstream ss_actual{optional_actual_serialized_nfa.value()};
    stringstream ss_expected{expected_serialized_nfa};
    string actual_line;
    string expected_line;

    CAPTURE(optional_actual_serialized_nfa.value());
    CAPTURE(expected_serialized_nfa);
    while (getline(ss_actual, actual_line) && getline(ss_expected, expected_line)) {
        REQUIRE(actual_line == expected_line);
    }
    getline(ss_actual, actual_line);
    REQUIRE(actual_line.empty());
    getline(ss_expected, expected_line);
    REQUIRE(expected_line.empty());
}
