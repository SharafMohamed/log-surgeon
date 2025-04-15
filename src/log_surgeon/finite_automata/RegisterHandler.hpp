#ifndef LOG_SURGEON_FINITE_AUTOMATA_REGISTER_HANDLER_HPP
#define LOG_SURGEON_FINITE_AUTOMATA_REGISTER_HANDLER_HPP

#include <cstdint>
#include <map>
#include <vector>

#include <log_surgeon/finite_automata/PrefixTree.hpp>
#include <log_surgeon/types.hpp>
#include <log_surgeon/UniqueIdGenerator.hpp>

namespace log_surgeon::finite_automata {
/**
 * The register handler maintains a prefix tree that is sufficient to represent all registers.
 * The register handler also contains a vector of registers, and performs the set, copy, and append
 * operations for these registers.
 *
 * NOTE: For efficiency, registers are not initialized when lexing a new string; instead, it is the
 * DFA's responsibility to set the register values when needed.
 */
class RegisterHandler {
public:
    auto add_registers(std::vector<bool> const& multi_valued_list) -> void {
        for (auto const multi_valued : multi_valued_list) {
            add_register(multi_valued);
        }
    }

    auto add_register(bool const multi_valued) -> reg_id_t {
        auto reg_id{m_reg_id_gen.generate_id()};
        m_multi_valued.emplace(reg_id, multi_valued);
        if (multi_valued) {
            m_multi_valued_registers.emplace_back(PrefixTree::cRootId);
        } else {
            m_single_valued_registers.emplace(reg_id, -1);
        }
        return reg_id;
    }

    auto
    copy_multi_valued_register(reg_id_t const dest_reg_id, reg_id_t const source_reg_id) -> void {
        m_multi_valued_registers.at(dest_reg_id) = m_multi_valued_registers.at(source_reg_id);
    }

    auto
    copy_single_valued_register(reg_id_t const dest_reg_id, reg_id_t const source_reg_id) -> void {
        m_single_valued_registers.at(dest_reg_id) = m_single_valued_registers.at(source_reg_id);
    }

    auto set_single_valued_position(reg_id_t const reg_id, reg_pos_t const position) -> void {
        // TODO: test performance of using []
        m_single_valued_registers.at(reg_id) = position;
    }

    auto append_multi_valued_position(reg_id_t const reg_id, reg_pos_t const position) -> void {
        auto const node_id{m_multi_valued_registers.at(reg_id)};
        m_multi_valued_registers.at(reg_id) = m_prefix_tree.insert(node_id, position);
    }

    [[nodiscard]] auto get_reversed_positions(reg_id_t const reg_id
    ) const -> std::vector<reg_pos_t> {
        if (m_multi_valued.at(reg_id)) {
            return m_prefix_tree.get_reversed_positions(m_multi_valued_registers.at(reg_id));
        }
        return {m_single_valued_registers.at(reg_id)};
    }

    [[nodiscard]] auto get_num_regs() const -> size_t { return m_reg_id_gen.get_num_ids(); }

    [[nodiscard]] auto get_multi_valued() const -> std::map<reg_id_t, bool> const& {
        return m_multi_valued;
    }

    // TODO: maybe faster to release and reininitialize
    // TODO: maybe faster if its a vector and not a map
    [[nodiscard]] auto get_multi_valued_registers() const -> std::vector<id_t> {
        return m_multi_valued_registers;
    }

    // TODO: maybe faster to release and reininitialize
    // TODO: maybe faster if its a vector and not a map
    [[nodiscard]] auto get_single_valued_registers() const -> std::map<reg_id_t, reg_pos_t> {
        return m_single_valued_registers;
    }

    [[nodiscard]] auto release_and_reset_prefix_tree() -> PrefixTree {
        PrefixTree moved_tree = std::move(m_prefix_tree);
        m_prefix_tree = PrefixTree();
        return moved_tree;
    }

private:
    UniqueIdGenerator m_reg_id_gen;
    PrefixTree m_prefix_tree;
    std::vector<PrefixTree::id_t> m_multi_valued_registers;
    std::map<reg_id_t, int32_t> m_single_valued_registers;
    std::map<reg_id_t, bool> m_multi_valued;
};
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_REGISTER_HANDLER_HPP
