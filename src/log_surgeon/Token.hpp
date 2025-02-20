#ifndef LOG_SURGEON_TOKEN_HPP
#define LOG_SURGEON_TOKEN_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace log_surgeon {
class Token {
public:
    Token() = default;

    Token(uint32_t const start_pos,
          uint32_t const end_pos,
          char const* buffer,
          uint32_t const buffer_size,
          uint32_t const line,
          std::vector<uint32_t> const* type_ids_ptrs)
            : m_start_pos(start_pos),
              m_end_pos(end_pos),
              m_buffer(buffer),
              m_buffer_size(buffer_size),
              m_line(line),
              m_type_ids_ptr(type_ids_ptrs) {}

    /**
     * @return The token's value as a string
     */
    [[nodiscard]] auto to_string() -> std::string;
    /**
     * @return A string view of the token's value
     */
    [[nodiscard]] auto to_string_view() -> std::string_view;

    /**
     * @return The first character (as a string) of the token string (which is a
     * delimiter if delimiters are being used)
     */
    [[nodiscard]] auto get_delimiter() const -> std::string;

    /**
     * @param i
     * @return The ith character of the token string
     */
    [[nodiscard]] auto get_char(uint8_t i) const -> char;

    /**
     * @return The length of the token string
     */
    [[nodiscard]] auto get_length() const -> uint32_t;

    uint32_t m_start_pos{0};
    uint32_t m_end_pos{0};
    char const* m_buffer{nullptr};
    uint32_t m_buffer_size{0};
    uint32_t m_line{0};
    std::vector<uint32_t> const* m_type_ids_ptr{nullptr};
    std::string m_wrap_around_string;
};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_TOKEN_HPP
