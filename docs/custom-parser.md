# Custom Parser

A custom parser specifies the variables, LALR1 grammar rules, and code fragments
necessary for `log-surgeon` to parse an input and provide meaningful output.
`log-surgeon` uses the variable rules to find and categorize tokens in the
input. These grammar rules are then used to reduce the tokens to a single symbol
(representing the desired language/interpretation). Each grammar rule has an
associated code fragment specifying what code to execute when the grammar rule
is used.

## Variable rule syntax

Each variable *rule* has a *name* and a *pattern* (regular expression). For
example, the following rules could be used when parsing json-like inputs: 

```
lbrace:{
rbrace:}
comma:,
equal:=
integer_token:[0-9]+
boolean_token:(true)|(false)
string_token:[^{},=]+
```

[CustomParser.cpp][1] has an example implementation of these variable rules:

```
void CustomParser::add_lexical_rules() {
    add_token("Lbrace", '{');
    add_token("Rbrace", '}');
    add_token("Comma", ',');
    add_token("Equal", '=');
    auto digit = make_unique<RegexASTGroupByte>('0', '9');
    auto digit_plus = make_unique<RegexASTMultiplicationByte>(std::move(digit), 1, 0);
    add_rule("IntegerToken", std::move(digit_plus));
    add_token_chain("BooleanToken", "true");
    add_token_chain("BooleanToken", "false");
    // default constructs to a m_negate group
    unique_ptr<RegexASTGroupByte> string_character = make_unique<RegexASTGroupByte>();
    string_character->add_literal(',');
    string_character->add_literal('=');
    string_character->add_literal('{');
    string_character->add_literal('}');
    unique_ptr<RegexASTMultiplicationByte> string_character_plus
            = make_unique<RegexASTMultiplicationByte>(std::move(string_character), 1, 0);
    add_rule("StringToken", std::move(string_character_plus));
}
```

## LALR1 grammar syntax

Each LALR1 grammar *rule* has a *head* and a *body*. The *head* of a grammar
rule is a single *symbol* called a *non-terminal*. The *body* is made of a
sequence of *symbols* either containing non-terminals or tokens. Below is
an example set of grammar rules for parsing json-like inputs (camel case for
tokens, pascal case for non-terminals) :

```    
JsonRecord --> JsonRecord comma GoodJsonObject
           --> GoodJsonObject
           --> JsonRecord comma BadJsonObject
           --> BadJsonObject
GoodJsonObject --> GoodJsonObject equal
               --> GoodJsonObject Value
               --> BadJsonObject equal
BadJsonObject --> BadJsonObject Value
              --> Value
Value --> stringToken
      --> lBrace JsonRecord rBrace
      --> lBrace rBrace
      --> booleanToken
      --> integerToken
```
As tokens are matched in the input they are pushed onto a stack. The LALR1 
parser uses the specified grammar to determine whether to *shift* and add
another token onto the stack or to *reduce* a matched sequence of symbols in the
*body* of a rule to the corresponding *head* symbol. This *shift*/*reduce*
decision is based on the next token to be added to the stack. A parse is
considered successful (e.g., the inout is determined to belong to the parser's
language) if all the symbols are reduced to a single symbol, specifically the
symbol specified in the first rule. Otherwise, the parse fails, and the input
does not belong to the desired language.

[CustomParser.cpp][1] has an example implementation of these grammar rules:

```    
add_production(
        "JsonRecord",
        {"JsonRecord", "Comma", "GoodJsonObject"},
        existing_json_record_rule
);
add_production("JsonRecord", {"GoodJsonObject"}, new_json_record_rule);
add_production(
        "JsonRecord",
        {"JsonRecord", "Comma", "BadJsonObject"},
        existing_json_record_rule
);
add_production("JsonRecord", {"BadJsonObject"}, new_json_record_rule);
add_production("GoodJsonObject", {"GoodJsonObject", "Equal"}, char_json_object_rule);
add_production("GoodJsonObject", {"GoodJsonObject", "Value"}, existing_json_object_rule);
add_production("GoodJsonObject", {"BadJsonObject", "Equal"}, new_good_json_object_rule);
add_production("BadJsonObject", {"BadJsonObject", "Value"}, existing_json_object_rule);
add_production(
        "BadJsonObject",
        {"Value"},
        std::bind(&CustomParser::bad_json_object_rule, this, std::placeholders::_1)
);
add_production("Value", {"StringToken"}, new_string_rule);
add_production("Value", {"Lbrace", "JsonRecord", "Rbrace"}, dictionary_rule);
add_production("Value", {"Lbrace", "Rbrace"}, empty_dictionary_rule);
add_production("Value", {"BooleanToken"}, boolean_rule);
add_production("Value", {"IntegerToken"}, integer_rule);
```

## Code fragments
The above two sections discuss the variable and grammar rules necessary to
determine if an input belongs to the parser's language. However, usually when
parsing logs, additional functionality is required. For example, in the case of
the json-like values, it may be desirable to have a json AST returned from the
parser. To have this additional functionality, a code fragment is associated 
with each grammar rule and executed when the rule is used to reduce symbols.

Example code fragments are demonstrated in [CustomParser.cpp][1]:

```
static auto new_string_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    string r1 = m->token_cast(0)->to_string();
    return make_unique<JsonValueAST>(r1, JsonValueType::String);
}

static auto existing_json_record_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    unique_ptr<ParserAST>& r1 = m->non_terminal_cast(0)->get_parser_ast();
    auto* r1_ptr = dynamic_cast<JsonRecordAST*>(r1.get());
    unique_ptr<ParserAST>& r3 = m->non_terminal_cast(2)->get_parser_ast();
    r1_ptr->add_object_ast(r3);
    return std::move(r1);
}
```
Each code fragment takes as an argument a non-terminal representing the *head*
of the associated grammar rule. This non-terminal contains a vector of
matched symbols corresponding to the symbols in the *body* of the grammar
rule. 

The new_string_rule() code fragment is used when generating a Value symbol
from a stringToken symbol. The code fragment accesses the stringToken symbol 
using m->token_cast(0). *token_cast* is used as the symbol is a token and index
0 indicates it is the first symbol in the *body*. 

The existing_json_record_rule() is used when adding additional JsonObjects to
the JsonRecord. The JsonRecord and the JsonObject in the *body* are accessed
using non_terminal_cast(0) and non_terminal_cast(2), respectively.
non_terminal_cast() is used because both symbols are non-terminal. Indexes 0 and
2 indicate the position of the record and object in the rule's body, 
respectively. The code fragments always return an updated symbol for the
grammar rule's *head*. This is then either used by another code fragment, or is
the final symbol returned to the user via the parse() call.

## Example Usage
Once the variable rules, grammar rules, and code fragments (along with any 
necessary objets needed by these fragments) are implemented for the desired
parser, the parser can be generated and used as follows:

```
log_surgeon:CustomParser custom_parser;
auto ast = custom_parser.parse_input(json_like_string);
auto* my_type_ast = dynamic_cast<MyTypeAST*>(ast.get());
```
The value returned from parse_input() is type ParserAST. To add functionality,
inherit from ParserAST to create MyTypeAST. After this, MyTypeAST can
be processed as desired. 

[custom-parser.cpp][2] contains examples of using a json-like parser to parse
json-like strings and print valid json. 

[1]: ../src/log_surgeon/CustomParser.cpp
[2]: ../examples/custom-parser.cpp

