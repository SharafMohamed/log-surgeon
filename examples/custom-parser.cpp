#include <iostream>
#include <string>
#include <vector>

#include <log_surgeon/CustomParser.hpp>

#include "common.hpp"

using std::string;
using namespace log_surgeon;

/// TODO: json_like_string ending in comma is not handled
auto main() -> int {
    std::string json_like_string = " empty=, some_text1, a_random_key1=10, a_random_key2=true, some_text2,"
                                   " a_random_key3=some_value, some_text3, empty=";
    CustomParser custom_parser;
    std::unique_ptr<JsonRecordAST> json_record_ast1
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "Input:" << json_like_string << std::endl;
    std::cout << "AST human readable output:" << json_record_ast1->print(true) << std::endl;
    json_like_string = "Request and Response Information, SOME_REDUCED_PAYLOAD=null,"
                       " someId=0e820f76-104d-4b1d-b93a-fc1837a63efa, duration=21, bool=true, "
                       "almost-bool2=truefalse, almost-bool2=truer, "
                       "fakeRespHeaders=FA_KE_ID=0:FAKE_LOCALE_ID=en_US:x-o-fake-id=0:FA_KER_ID=0";
    std::unique_ptr<JsonRecordAST> json_record_ast2
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "Input:" << json_like_string << std::endl;
    std::cout << "AST human readable output:" << json_record_ast2->print(true) << std::endl;

    return 0;
}
