#ifndef LOG_SURGEON_PARSERAST_HPP
#define LOG_SURGEON_PARSERAST_HPP

#include <stdexcept>
#include <string>
#include <utility>

namespace log_surgeon {
template <typename T>
class ParserValue;

class ParserAST {
public:
    virtual ~ParserAST() = 0;

    ParserAST(ParserAST const& rhs) = default;
    auto operator=(ParserAST const& rhs) -> ParserAST& = default;
    ParserAST(ParserAST&& rhs) noexcept = delete;
    auto operator=(ParserAST&& rhs) noexcept -> ParserAST& = delete;

    template <typename T>
    auto get() -> T& {
        auto* parser_value{dynamic_cast<ParserValue<T>*>(this)};
        if (nullptr == parser_value) {
            throw std::invalid_argument(
                    "Failed to cast `" + std::string(typeid(*this).name()) + "` to `"
                    + std::string(typeid(T).name()) + "`."
            );
        }
        return parser_value->m_value;
    }
};

template <typename T>
class ParserValue : public ParserAST {
public:
    T m_value;

    explicit ParserValue(T val) : m_value(std::move(val)) {}
};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_PARSERAST_HPP
