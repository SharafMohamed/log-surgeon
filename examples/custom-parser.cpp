#include <iostream>
#include <string>
#include <vector>

#include <log_surgeon/CustomParser.hpp>

#include "common.hpp"

using std::string;
using namespace log_surgeon;

/// TODO: json_like_string ending in comma is not handled
auto main() -> int {
    std::string json_like_string
            = " empty=, empty_dict={}, some_text1, a_random_key1=10, a_random_key2=true, some_text2,"
              " a_random_key3=some_value, some_text3, empty=, a_random_key4=123asd, "
              "a_random_key4==false";
    CustomParser custom_parser;
    std::unique_ptr<JsonRecordAST> json_record_ast1
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast1->print(true) << std::endl
              << std::endl;

    json_like_string = "Request and Response Information, SOME_REDUCED_PAYLOAD=null,"
                       " someId=0e820f76-104d-4b1d-b93a-fc1837a63efa, duration=21, bool=true, "
                       "almost-bool2=truefalse, almost-bool2=truer, "
                       "fakeRespHeaders=FA_KE_ID=0:FAKE_LOCALE_ID=en_US:x-o-fake-id=0:FA_KER_ID=0, "
                       "equal==123, equalint=123=123, equalbool=true=false";
    std::unique_ptr<JsonRecordAST> json_record_ast2
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast2->print(true) << std::endl
              << std::endl;

    json_like_string
            = "level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
              "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
              "a\\\"}";

    std::unique_ptr<JsonRecordAST> json_record_ast3
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast3->print(true) << std::endl
              << std::endl;

    json_like_string
            = "LogFuncArg(level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
              "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
              "a\\\"})";

    std::unique_ptr<JsonRecordAST> json_record_ast4
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast4->print(true) << std::endl
              << std::endl;
    return 0;
}
