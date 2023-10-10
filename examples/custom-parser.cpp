#include <iostream>
#include <string>
#include <vector>

#include <log_surgeon/CustomParser.hpp>

#include "common.hpp"

using std::string;
using namespace log_surgeon;

auto main() -> int {
    std::string json_like_string = " some_text1, a_random=_key1=10, a_random_key2=true, some_text2,"
                                   " a_random_key3=some_value, some_text3";
    CustomParser custom_parser;
    std::unique_ptr<JsonRecordAST> json_record_ast = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "Input:" << json_like_string << std::endl;
    std::cout << "AST human readable output:" << json_record_ast->print() << std::endl;
    return 0;
}

/// TODO: 
/// 1. If it fails to parse it shouldn't terminate with an error, it should return an error instead
/// 2. Add key to bad_json
/// 3. For asd=asd=asd case treat fire = as the Key-Value delimiter
/// 4. Integrate into uslope