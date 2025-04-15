#ifndef LOG_SURGEON_TOKEN_HPP
#define LOG_SURGEON_TOKEN_HPP

#include <string>
#include <string_view>
#include <vector>

#include <log_surgeon/finite_automata/PrefixTree.hpp>
#include <log_surgeon/finite_automata/RegisterHandler.hpp>
#include <log_surgeon/types.hpp>

namespace log_surgeon {
class Token {
public:
    auto
    assign_regs(finite_automata::RegisterHandler& reg_handler, bool const is_reptition) -> void {
        m_single_valued_registers = reg_handler.get_single_valued_registers();
        // TODO: only do these 2 steps if the rule has reptition
        if (is_reptition) {
            m_multi_valued_registers = reg_handler.get_multi_valued_registers();
            m_prefix_tree = reg_handler.release_and_reset_prefix_tree();
        }
        // TODO: this doesn't need to be done each time as its a lexer constant:
        m_multi_valued = reg_handler.get_multi_valued();
    }

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

    [[nodiscard]] auto get_reg_positions(reg_id_t const reg_id) const -> std::vector<reg_pos_t> {
        if (m_multi_valued.at(reg_id)) {
            return m_prefix_tree.get_reversed_positions(m_multi_valued_registers.at(reg_id));
        }
        return {m_single_valued_registers.at(reg_id)};
    }

    uint32_t m_start_pos{0};
    uint32_t m_end_pos{0};
    char const* m_buffer{nullptr};
    uint32_t m_buffer_size{0};
    uint32_t m_line{0};
    std::vector<uint32_t> const* m_type_ids_ptr{nullptr};
    std::string m_wrap_around_string{};
    finite_automata::PrefixTree m_prefix_tree{};
    std::vector<finite_automata::PrefixTree::id_t> m_multi_valued_registers{};
    std::map<reg_id_t, int32_t> m_single_valued_registers{};
    std::map<reg_id_t, bool> m_multi_valued{};
};
}  // namespace log_surgeon

#endif  // LOG_SURGEON_TOKEN_HPP
