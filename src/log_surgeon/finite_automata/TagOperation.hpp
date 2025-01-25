#ifndef LOG_SURGEON_FINITE_AUTOMATA_TAGOPERATION_HPP
#define LOG_SURGEON_FINITE_AUTOMATA_TAGOPERATION_HPP

#include <cstdint>
#include <string>

#include <fmt/core.h>

namespace log_surgeon::finite_automata {
using tag_id_t = uint32_t;

enum class TagOperationType : uint8_t {
    Set,
    Negate
};

class TagOperation {
public:
    TagOperation(tag_id_t const tag_id, TagOperationType const type)
            : m_tag_id{tag_id},
              m_type{type} {}

    [[nodiscard]] auto get_tag_id() const -> tag_id_t { return m_tag_id; }

    [[nodiscard]] auto get_type() const -> TagOperationType { return m_type; }

    /**
     * @return A string representation of the tag operation.
     */
    [[nodiscard]] auto serialize() const -> std::string {
        switch (m_type) {
            case TagOperationType::Set:
                return fmt::format("{}{}", m_tag_id, "p");
            case TagOperationType::Negate:
                return fmt::format("{}{}", m_tag_id, "n");
            default:
                return fmt::format("{}{}", m_tag_id, "?");
        }
    }

private:
    tag_id_t m_tag_id;
    TagOperationType m_type;
};
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_TAGOPERATION_HPP
