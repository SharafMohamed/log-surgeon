#ifndef LOG_SURGEON_FINITE_AUTOMATA_REGISTEROPERATION_HPP
#define LOG_SURGEON_FINITE_AUTOMATA_REGISTEROPERATION_HPP

#include <cstdint>
#include <optional>
#include <string>

#include <fmt/core.h>

#include <log_surgeon/types.hpp>

namespace log_surgeon::finite_automata {
/**
 * Represents a register operation:
 * - A register ID specifying which register the operation applies to.
 * - An operation type: `Copy`, `Set`, or `Negate`.
 * - An optional source register ID for copy operations.
 */
class RegisterOperation {
public:
    enum class Type : uint8_t {
        Set,
        Append,
        NegateSet,
        NegateAppend,
        CopySet,
        CopyAppend
    };

    bool operator==(RegisterOperation const& rhs) const {
        return m_reg_id == rhs.m_reg_id && m_type == rhs.m_type
               && m_copy_reg_id == rhs.m_copy_reg_id;
    }

    static auto
    create_set_operation(reg_id_t const reg_id, bool const multi_valued) -> RegisterOperation {
        if (multi_valued) {
            return {reg_id, Type::Append, multi_valued};
        }
        return {reg_id, Type::Set, multi_valued};
    }

    static auto
    create_negate_operation(reg_id_t const reg_id, bool const multi_valued) -> RegisterOperation {
        if (multi_valued) {
            return {reg_id, Type::NegateAppend, multi_valued};
        }
        return {reg_id, Type::NegateSet, multi_valued};
    }

    static auto create_copy_operation(
            reg_id_t const dest_reg_id,
            reg_id_t const src_reg_id,
            bool const multi_valued
    ) -> RegisterOperation {
        if (multi_valued) {
            return {dest_reg_id, src_reg_id, Type::CopyAppend, multi_valued};
        }
        return {dest_reg_id, src_reg_id, Type::CopySet, multi_valued};
    }

    auto set_reg_id(reg_id_t const reg_id) -> void { m_reg_id = reg_id; }

    [[nodiscard]] auto get_reg_id() const -> reg_id_t { return m_reg_id; }

    [[nodiscard]] auto get_type() const -> Type { return m_type; }

    [[nodiscard]] auto get_copy_reg_id() const -> std::optional<reg_id_t> { return m_copy_reg_id; }

    [[nodiscard]] auto is_multi_valued() const -> bool { return m_multi_valued; }

    /**
     * Serializes the register operation into a string representation.
     *
     * @return A formatted string encoding the operation.
     * @return `std::nullopt` if:
     * - the operation type is invalid.
     * - no source register is specified for a copy operation.
     */
    [[nodiscard]] auto serialize() const -> std::optional<std::string> {
        switch (m_type) {
            case Type::Set:
                return fmt::format("{}{}", m_reg_id, "p");
            case Type::Append:
                return fmt::format("{}{}", m_reg_id, "p+");
            case Type::NegateSet:
                return fmt::format("{}{}", m_reg_id, "n");
            case Type::NegateAppend:
                return fmt::format("{}{}", m_reg_id, "n+");
            case Type::CopySet:
                if (false == m_copy_reg_id.has_value()) {
                    return std::nullopt;
                }
                return fmt::format("{}{}{}", m_reg_id, "c", m_copy_reg_id.value());
            case Type::CopyAppend:
                if (false == m_copy_reg_id.has_value()) {
                    return std::nullopt;
                }
                return fmt::format("{}{}{}", m_reg_id, "c+", m_copy_reg_id.value());
            default:
                return std::nullopt;
        }
    }

private:
    RegisterOperation(reg_id_t const reg_id, Type const type, bool const multi_valued)
            : m_reg_id{reg_id},
              m_type{type},
              m_multi_valued{multi_valued} {}

    RegisterOperation(
            reg_id_t const reg_id,
            reg_id_t const copy_reg_id,
            Type const type,
            bool const multi_valued
    )
            : m_reg_id{reg_id},
              m_type{type},
              m_copy_reg_id{copy_reg_id},
              m_multi_valued{multi_valued} {}

    reg_id_t m_reg_id;
    Type m_type;
    std::optional<reg_id_t> m_copy_reg_id{std::nullopt};
    bool m_multi_valued;
};
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_REGISTEROPERATION_HPP
