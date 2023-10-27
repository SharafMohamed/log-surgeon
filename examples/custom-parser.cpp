#include <iostream>
#include <string>
#include <vector>

#include <log_surgeon/CustomParser.hpp>

#include "common.hpp"

using std::string;
using namespace log_surgeon;

/// TODO: json_like_string ending in comma is not handled
auto main() -> int {
    CustomParser custom_parser;

    std::string json_like_string
            = "empty=,empty_dict = {}, some_text1 , a_random_key1=10, a_random_key2=true,"
              " some_text2, a_random_key3=some_value, some_text3, empty=, a_random_key4=123abc, "
              "a_random_key4==false =abc= ";
    auto ast1 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast1 = static_cast<JsonRecordAST*>(ast1.get());
    std::cout << "AST human readable output:" << json_record_ast1->print(true) << std::endl
              << std::endl;

    json_like_string = "Request and Response Information, SOME_REDUCED_PAYLOAD=null,"
                       " someId=0e820f76-104d-4b1d-b93a-fc1837a63efa, duration=21, bool=true, "
                       "almost-bool2=truefalse, almost-bool2=truer, "
                       "fakeRespHeaders=FA_KE_ID=0:FAKE_LOCALE_ID=en_US:x-o-fake-id=0:FA_KER_ID=0, "
                       "equal==123, equalint=123=123, equalbool=true=false";
    auto ast2 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast2 = static_cast<JsonRecordAST*>(ast2.get());
    std::cout << "AST human readable output:" << json_record_ast2->print(true) << std::endl
              << std::endl;

    json_like_string = "level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
                       "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
                       "a\\\"}";

    auto ast3 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast3 = static_cast<JsonRecordAST*>(ast3.get());
    std::cout << "AST human readable output:" << json_record_ast3->print(true) << std::endl
              << std::endl;

    json_like_string
            = "LogFuncArg(level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
              "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
              "a\\\"})";
    auto ast4 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast4 = static_cast<JsonRecordAST*>(ast4.get());
    std::cout << "AST human readable output:" << json_record_ast4->print(true) << std::endl
              << std::endl;

    json_like_string = " Key = { } ,log={ test =123 , text =  },,, log2={,,,}, log3={ , , ,  }, "
                       "log4={{{,{,}}{}},,}}} ";
    auto ast5 = custom_parser.parse_input(json_like_string);
    auto* json_record_ast5 = static_cast<JsonRecordAST*>(ast5.get());
    std::cout << "AST human readable output:" << json_record_ast5->print(true) << std::endl
              << std::endl;
    return 0;
}
