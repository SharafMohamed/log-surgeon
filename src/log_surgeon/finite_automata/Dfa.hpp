#ifndef LOG_SURGEON_FINITE_AUTOMATA_DFA_HPP
#define LOG_SURGEON_FINITE_AUTOMATA_DFA_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>

#include <log_surgeon/Constants.hpp>
#include <log_surgeon/finite_automata/DeterminizationConfiguration.hpp>
#include <log_surgeon/finite_automata/DfaStatePair.hpp>
#include <log_surgeon/finite_automata/Nfa.hpp>
#include <log_surgeon/finite_automata/RegisterHandler.hpp>
#include <log_surgeon/finite_automata/RegisterOperation.hpp>
#include <log_surgeon/finite_automata/TagOperation.hpp>
#include <log_surgeon/Token.hpp>

namespace log_surgeon::finite_automata {
/**
 * Represents a Deterministic Finite Automaton (DFA).
 *
 * The DFA is constructed from an NFA using the superset determinization algorithm. The DFA consists
 * of states, transitions, and registers for tracking tagged captures.
 *
 * @tparam TypedDfaState The type representing a DFA state.
 * @tparam TypedNfaState The type representing an NFA state.
 */
template <typename TypedDfaState, typename TypedNfaState>
class Dfa {
public:
    using ConfigurationSet = std::set<DeterminizationConfiguration<TypedNfaState>>;

    explicit Dfa(Nfa<TypedNfaState> const& nfa);

    auto reset() -> void { m_curr_state = get_root(); }

    /**
     * Determine the out-going transition  based on the input character. Update the current state
     * and register values based on the transition.
     * @param next_char The character to transition on.
     * @param curr_pos The current position in the lexing.
     * @return The destination state.
     *
     *
     * This is what it should return, but for backward compatability its doing the above atm.
     * @return The destination state when it is accepting
     * @return `std::nullopt` when the state is not accepting.
     * @return `nullptr` when the input leads to a non-matching sequence.
     *
     *
     * @throws `std::logic_error` if copy operation has no source register.
     * @throws `std::logic_error` if register operation has unhandlded type.
     */
    auto process_char(uint32_t next_char, uint32_t curr_pos) -> TypedDfaState const*;

    /**
     * Applies the register operations for the accepting state.
     * @param dfa_state An accepting DFA state.
     * @param curr_pos The current position in the lexing.
     */
    auto process_state(TypedDfaState const* dfa_state, uint32_t curr_pos) -> void;

    auto set(TypedDfaState const* prev_state) -> TypedDfaState const* {
        m_curr_state = prev_state;
        return m_curr_state;
    }

    /**
     * @return A string representation of the DFA.
     * @return Forwards `DfaState::serialize`'s return value (std::nullopt) on failure.
     */
    [[nodiscard]] auto serialize() const -> std::optional<std::string>;

    [[nodiscard]] auto get_root() const -> TypedDfaState const* { return m_states.at(0).get(); }

    /**
     * Compares this DFA with `dfa_in` to determine the set of schema types in this DFA that are
     * reachable by any type in `dfa_in`. A type is considered reachable if there is at least one
     * string for which: (1) this DFA returns a set of types containing the type, and (2) `dfa_in`
     * returns any non-empty set of types.
     *
     * @param dfa_in The DFA with which to compute the intersection.
     * @return The set of schema types reachable by `dfa_in`.
     */
    [[nodiscard]] auto get_intersect(Dfa const* dfa_in) const -> std::set<uint32_t>;

    [[nodiscard]] auto get_tag_id_to_final_reg_id() const -> std::map<tag_id_t, reg_id_t> {
        return m_tag_id_to_final_reg_id;
    }

    auto assign_token_regs(Token& token, bool const is_reptition ) -> void {
        token.assign_regs(m_reg_handler,  is_reptition);
    }

private:
    /**
     * Generates the DFA states from the given NFA using the superset determinization algorithm.
     *
     * @param nfa The NFA used to generate the DFA.
     */
    auto generate(Nfa<TypedNfaState> const& nfa) -> void;

    /**
     * Adds a register for tracking the initial and final value of each tag.
     *
     * @param multi_valued_list A list specifying if each registers to be added is multi-valued.
     * @param register_handler Returns the handler with the added registers.
     * @param initial_tag_id_to_reg_id Returns mapping of tag id to initial register id.
     * @param final_tag_id_to_reg_id Returns mapping of tag id to final register id.
     */
    static auto initialize_registers(
            std::vector<bool> multi_valued_list,
            RegisterHandler& register_handler,
            std::map<tag_id_t, reg_id_t>& initial_tag_id_to_reg_id,
            std::map<tag_id_t, reg_id_t>& final_tag_id_to_reg_id
    ) -> void;

    /**
     * Tries to find a single register mapping such that each config in `lhs` can be mapped to a
     * config in `rhs`. A config is considered mapped if both contain the same start, history, and
     * registers.
     *
     * @param lhs The first set of configurations.
     * @param rhs The second set of configurations.
     * @return The register mapping if a bijection is possible.
     * @return std::nullopt otherwise.
     */
    [[nodiscard]] static auto try_get_mapping(
            ConfigurationSet const& lhs,
            ConfigurationSet const& rhs
    ) -> std::optional<std::unordered_map<reg_id_t, reg_id_t>>;

    /**
     * Creates a DFA state based on the given config set if the config does not already exist and
     * cannot be mapped to an existing config. In the case of a new DFA state, it is added to
     * `m_states`, `dfa_states`, and `unexplored_sets`.
     *
     * @param config_set The configuration set for which to create or get the DFA state.
     * @param dfa_states Returns an updated map of configuration sets to DFA states.
     * @param unexplored_sets Returns a queue of unexplored states.
     * @param multi_valued A list specifying if each registers to be added is multi-valued.
     * @return If `new_config_set` is already in `dfa_states`, a pair containing:
     * - The existing DFA state.
     * - std::nullopt.
     * @return If `new_config_set` can be mapped to an existing config in `dfa_states`, a pair
     * containing:
     * - The existing DFA state.
     * - The register mapping.
     * @return Otherwise, a pair containing:
     * - The newly created DFA state.
     * - std::nullopt.
     */
    auto create_or_get_dfa_state(
            ConfigurationSet const& config_set,
            std::map<ConfigurationSet, TypedDfaState*>& dfa_states,
            std::queue<ConfigurationSet>& unexplored_sets,
            std::vector<bool> multi_valued
    ) -> std::pair<TypedDfaState*, std::optional<std::unordered_map<reg_id_t, reg_id_t>>>;

    /**
     * Determines the out-going transitions from the configuration set based on its NFA states.
     *
     * @param num_tags Number of tags in the NFA.
     * @param config_set The configuration set.
     * @param tag_id_with_op_to_reg_id Returns an updated mapping from operation tag id to
     * register id.
     * @return A map of input symbols to transitions. Each transition contains a vector of register
     * operations and a destination configuration set.
     */
    [[nodiscard]] auto get_transitions(
            size_t num_tags,
            ConfigurationSet const& config_set,
            std::map<tag_id_t, reg_id_t>& tag_id_with_op_to_reg_id
    ) -> std::map<uint32_t, std::pair<std::vector<RegisterOperation>, ConfigurationSet>>;

    /**
     * Iterates over the configurations in the closure to:
     * - Add the new registers needed to track the tags to 'm_reg_handler'.
     * - Determine the operations to perform on the new registers.
     *
     * @param num_tags Number of tags in the NFA.
     * @param closure Returns the set of DFA configurations with updated `tag_to_reg_ids`.
     * @param tag_id_with_op_to_reg_id Returns the updated map of tags with operations to
     * registers.
     * @returns The operations to perform on the new registers.
     */
    auto assign_transition_reg_ops(
            size_t num_tags,
            ConfigurationSet& closure,
            std::map<tag_id_t, reg_id_t>& tag_id_with_op_to_reg_id
    ) -> std::vector<RegisterOperation>;

    /**
     * Updates register operations by using the registers mapping to either modify existing
     * set/negate operations or create new copy operations if necessary. This helps ensure that the
     * destination state of the transition uses an existing DFA state, preventing the creation of
     * new DFA states and avoiding non-converging determinization.
     *
     * @param reg_map The register mapping used to update the register operations.
     * @param reg_ops Returns the updated vector of register operations.
     */
    static auto reassign_transition_reg_ops(
            std::unordered_map<reg_id_t, reg_id_t> const& reg_map,
            std::vector<RegisterOperation>& reg_ops
    ) -> void;

    /**
     * Creates a new DFA state based on a set of NFA configurations and adds it to `m_states`.
     *
     * @param config_set The set of configurations represented by this DFA state.
     * @param tag_id_to_final_reg_id Mapping from tag IDs to final register IDs.
     * @param multi_valued A list specifying if each registers to be added is multi-valued.
     * @return A pointer to the new DFA state.
     */
    [[nodiscard]] auto new_state(
            ConfigurationSet const& config_set,
            std::map<tag_id_t, reg_id_t> const& tag_id_to_final_reg_id,
            std::vector<bool> multi_valued
    ) -> TypedDfaState*;

    /**
     * @return A vector representing the traversal order of the DFA states using breadth-first
     * search (BFS).
     */
    [[nodiscard]] auto get_bfs_traversal_order() const -> std::vector<TypedDfaState const*>;

    std::vector<std::unique_ptr<TypedDfaState>> m_states;
    std::map<tag_id_t, reg_id_t> m_tag_id_to_final_reg_id;
    RegisterHandler m_reg_handler;
    TypedDfaState const* m_curr_state;
    size_t m_num_regs{0};
};

template <typename TypedDfaState, typename TypedNfaState>
Dfa<TypedDfaState, TypedNfaState>::Dfa(Nfa<TypedNfaState> const& nfa) : m_curr_state{nullptr} {
    generate(nfa);
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::process_char(
        uint32_t const next_char,
        uint32_t const curr_pos
) -> TypedDfaState const* {
    auto const optional_transition{m_curr_state->get_transition(next_char)};
    if (false == optional_transition.has_value()) {
        return nullptr;
    }
    m_curr_state = optional_transition.value().get_dest_state();

    auto const reg_ops{optional_transition.value().get_reg_ops()};
    for (auto const& reg_op : reg_ops) {
        switch (reg_op.get_type()) {
            case RegisterOperation::Type::Set: {
                m_reg_handler.set_single_valued_position(reg_op.get_reg_id(), curr_pos);
                break;
            }
            case RegisterOperation::Type::Append: {
                m_reg_handler.append_multi_valued_position(reg_op.get_reg_id(), curr_pos);
                break;
            }
            case RegisterOperation::Type::NegateSet: {
                m_reg_handler.set_single_valued_position(reg_op.get_reg_id(), -1);
                break;
            }
            case RegisterOperation::Type::NegateAppend: {
                m_reg_handler.append_multi_valued_position(reg_op.get_reg_id(), -1);
                break;
            }
            case RegisterOperation::Type::CopySet: {
                auto copy_reg_id_optional{reg_op.get_copy_reg_id()};
                if (copy_reg_id_optional.has_value()) {
                    m_reg_handler.copy_single_valued_register(
                            reg_op.get_reg_id(),
                            copy_reg_id_optional.value()
                    );
                } else {
                    throw std::logic_error("Copy operation does not specify register to copy.");
                }
                break;
            }
            case RegisterOperation::Type::CopyAppend: {
                auto copy_reg_id_optional{reg_op.get_copy_reg_id()};
                if (copy_reg_id_optional.has_value()) {
                    m_reg_handler.copy_multi_valued_register(
                            reg_op.get_reg_id(),
                            copy_reg_id_optional.value()
                    );
                } else {
                    throw std::logic_error("Copy operation does not specify register to copy.");
                }
                break;
            }
            default: {
                throw std::logic_error("Unhandled register operation type when simulating DFA.");
            }
        }
    }
    return m_curr_state;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::process_state(
        TypedDfaState const* dfa_state,
        uint32_t const curr_pos
) -> void {
    auto const reg_ops{dfa_state->get_accepting_reg_ops()};
    for (auto const& reg_op : reg_ops) {
        switch (reg_op.get_type()) {
            case RegisterOperation::Type::Set: {
                m_reg_handler.set_single_valued_position(reg_op.get_reg_id(), curr_pos);
                break;
            }
            case RegisterOperation::Type::Append: {
                m_reg_handler.append_multi_valued_position(reg_op.get_reg_id(), curr_pos);
                break;
            }
            case RegisterOperation::Type::NegateSet: {
                m_reg_handler.set_single_valued_position(reg_op.get_reg_id(), -1);
                break;
            }
            case RegisterOperation::Type::NegateAppend: {
                m_reg_handler.append_multi_valued_position(reg_op.get_reg_id(), -1);
                break;
            }
            case RegisterOperation::Type::CopySet: {
                auto copy_reg_id_optional{reg_op.get_copy_reg_id()};
                if (copy_reg_id_optional.has_value()) {
                    m_reg_handler.copy_single_valued_register(
                            reg_op.get_reg_id(),
                            copy_reg_id_optional.value()
                    );
                } else {
                    throw std::logic_error("Copy operation does not specify register to copy.");
                }
                break;
            }
            case RegisterOperation::Type::CopyAppend: {
                auto copy_reg_id_optional{reg_op.get_copy_reg_id()};
                if (copy_reg_id_optional.has_value()) {
                    m_reg_handler.copy_multi_valued_register(
                            reg_op.get_reg_id(),
                            copy_reg_id_optional.value()
                    );
                } else {
                    throw std::logic_error("Copy operation does not specify register to copy.");
                }
                break;
            }
            default: {
                throw std::logic_error("Unhandled register operation type when simulating DFA.");
            }
        }
    }
}

// TODO: handle utf8 case in DFA generation.
template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::generate(Nfa<TypedNfaState> const& nfa) -> void {
    std::map<tag_id_t, reg_id_t> initial_tag_id_to_reg_id;
    initialize_registers(
            nfa.get_multi_valued(),
            m_reg_handler,
            initial_tag_id_to_reg_id,
            m_tag_id_to_final_reg_id
    );
    DeterminizationConfiguration<TypedNfaState>
            initial_config{nfa.get_root(), initial_tag_id_to_reg_id, {}, {}};

    std::map<ConfigurationSet, TypedDfaState*> dfa_states;
    std::queue<ConfigurationSet> unexplored_sets;
    create_or_get_dfa_state(
            initial_config.spontaneous_closure(),
            dfa_states,
            unexplored_sets,
            nfa.get_multi_valued()
    );
    while (false == unexplored_sets.empty()) {
        auto config_set{unexplored_sets.front()};
        auto* dfa_state{dfa_states.at(config_set)};
        unexplored_sets.pop();
        std::map<tag_id_t, reg_id_t> tag_id_with_op_to_reg_id;
        for (auto [ascii_value, dest_config_pair] :
             get_transitions(nfa.get_num_tags(), config_set, tag_id_with_op_to_reg_id))
        {
            auto& [reg_ops, dest_config_set]{dest_config_pair};
            auto [dest_state, optional_reg_map]{create_or_get_dfa_state(
                    dest_config_set,
                    dfa_states,
                    unexplored_sets,
                    nfa.get_multi_valued()
            )};
            if (optional_reg_map.has_value()) {
                reassign_transition_reg_ops(optional_reg_map.value(), reg_ops);
            }
            dfa_state->add_byte_transition(ascii_value, {reg_ops, dest_state});
        }
    }
    m_num_regs = m_reg_handler.get_num_regs();
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::initialize_registers(
        std::vector<bool> multi_valued_list,
        RegisterHandler& register_handler,
        std::map<tag_id_t, reg_id_t>& initial_tag_id_to_reg_id,
        std::map<tag_id_t, reg_id_t>& final_tag_id_to_reg_id
) -> void {
    size_t const num_tags{multi_valued_list.size()};
    multi_valued_list.reserve(2 * num_tags);
    multi_valued_list
            .insert(multi_valued_list.end(), multi_valued_list.begin(), multi_valued_list.end());
    register_handler.add_registers(multi_valued_list);
    for (uint32_t i{0}; i < num_tags; i++) {
        initial_tag_id_to_reg_id.insert({i, i});
        final_tag_id_to_reg_id.insert({i, num_tags + i});
    }
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::try_get_mapping(
        ConfigurationSet const& lhs,
        ConfigurationSet const& rhs
) -> std::optional<std::unordered_map<reg_id_t, reg_id_t>> {
    if (lhs.size() != rhs.size()) {
        return std::nullopt;
    }
    std::unordered_map<reg_id_t, reg_id_t> reg_map_lhs_to_rhs;
    std::unordered_map<reg_id_t, reg_id_t> reg_map_rhs_to_lhs;
    for (auto const& config_lhs : lhs) {
        bool found{false};
        for (auto const& config_rhs : rhs) {
            if (config_lhs.get_state() != config_rhs.get_state()
                || config_lhs.get_lookahead() != config_rhs.get_lookahead())
            {
                continue;
            }
            for (auto const [tag_id, lhs_reg_id] : config_lhs.get_tag_id_to_reg_ids()) {
                // If the NFA state sets the tag then the current register is irrelevent
                if (config_lhs.get_tag_lookahead(tag_id).has_value()) {
                    continue;
                }
                auto const rhs_reg_id{config_rhs.get_tag_id_to_reg_ids().at(tag_id)};
                if (false == reg_map_lhs_to_rhs.contains(lhs_reg_id)
                    && false == reg_map_rhs_to_lhs.contains(rhs_reg_id))
                {
                    reg_map_lhs_to_rhs.insert({lhs_reg_id, rhs_reg_id});
                    reg_map_rhs_to_lhs.insert({rhs_reg_id, lhs_reg_id});
                } else if (false == reg_map_lhs_to_rhs.contains(lhs_reg_id)
                           || false == reg_map_rhs_to_lhs.contains(rhs_reg_id)
                           || reg_map_lhs_to_rhs.at(lhs_reg_id) != rhs_reg_id
                           || reg_map_rhs_to_lhs.at(rhs_reg_id) != lhs_reg_id)
                {
                    return std::nullopt;
                }
            }
            found = true;
            break;
        }
        if (false == found) {
            return std::nullopt;
        }
    }
    return reg_map_lhs_to_rhs;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::create_or_get_dfa_state(
        ConfigurationSet const& config_set,
        std::map<ConfigurationSet, TypedDfaState*>& dfa_states,
        std::queue<ConfigurationSet>& unexplored_sets,
        std::vector<bool> const multi_valued
) -> std::pair<TypedDfaState*, std::optional<std::unordered_map<reg_id_t, reg_id_t>>> {
    if (false == dfa_states.contains(config_set)) {
        for (auto const& [config_set_in_map, dfa_state] : dfa_states) {
            auto const optional_reg_map{try_get_mapping(config_set, config_set_in_map)};
            if (optional_reg_map.has_value()) {
                return {dfa_state, optional_reg_map};
            }
        }
        dfa_states.insert(
                {config_set, new_state(config_set, m_tag_id_to_final_reg_id, multi_valued)}
        );
        unexplored_sets.push(config_set);
    }
    return {dfa_states.at(config_set), std::nullopt};
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::get_transitions(
        size_t const num_tags,
        ConfigurationSet const& config_set,
        std::map<tag_id_t, reg_id_t>& tag_id_with_op_to_reg_id
) -> std::map<uint32_t, std::pair<std::vector<RegisterOperation>, ConfigurationSet>> {
    std::map<uint32_t, std::pair<std::vector<RegisterOperation>, ConfigurationSet>>
            ascii_transitions_map;
    for (auto const& configuration : config_set) {
        auto const* nfa_state{configuration.get_state()};
        for (uint32_t i{0}; i < cSizeOfByte; ++i) {
            for (auto const* next_nfa_state : nfa_state->get_byte_transitions(i)) {
                DeterminizationConfiguration<TypedNfaState> next_configuration{
                        next_nfa_state,
                        configuration.get_tag_id_to_reg_ids(),
                        configuration.get_lookahead(),
                        {}
                };
                auto closure{next_configuration.spontaneous_closure()};
                auto const new_reg_ops{
                        assign_transition_reg_ops(num_tags, closure, tag_id_with_op_to_reg_id)
                };
                if (ascii_transitions_map.contains(i)) {
                    for (auto const& new_reg_op : new_reg_ops) {
                        auto& byte_reg_ops{ascii_transitions_map.at(i).first};
                        if (byte_reg_ops.end()
                            == std::find(byte_reg_ops.begin(), byte_reg_ops.end(), new_reg_op))
                        {
                            byte_reg_ops.push_back(new_reg_op);
                        }
                    }
                    ascii_transitions_map.at(i).second.insert(closure.begin(), closure.end());
                } else {
                    ascii_transitions_map.emplace(i, std::make_pair(new_reg_ops, closure));
                }
            }
        }
    }
    return ascii_transitions_map;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::assign_transition_reg_ops(
        size_t const num_tags,
        ConfigurationSet& closure,
        std::map<tag_id_t, reg_id_t>& tag_id_with_op_to_reg_id
) -> std::vector<RegisterOperation> {
    std::vector<RegisterOperation> reg_ops;
    std::set<DeterminizationConfiguration<TypedNfaState>> new_closure;

    std::map<reg_id_t, RegisterOperation> multi_valued_registers;
    std::vector<RegisterOperation> single_valued_registers;

    for (auto config : closure) {
        for (tag_id_t tag_id{0}; tag_id < num_tags; tag_id++) {
            auto const optional_tag_op{config.get_tag_history(tag_id)};
            if (optional_tag_op.has_value()) {
                auto const tag_op{optional_tag_op.value()};
                if (false == tag_id_with_op_to_reg_id.contains(tag_id)) {
                    tag_id_with_op_to_reg_id.emplace(
                            tag_id,
                            m_reg_handler.add_register(tag_op.is_multi_valued())
                    );
                }

                auto reg_id = tag_id_with_op_to_reg_id.at(tag_id);

                if (std::none_of(
                            reg_ops.begin(),
                            reg_ops.end(),
                            [reg_id](RegisterOperation const& reg_op) {
                                return reg_op.get_reg_id() == reg_id;
                            }
                    ))
                {
                    if (tag_op.is_multi_valued()) {
                        if (TagOperationType::Set == tag_op.get_type()) {
                            reg_ops.emplace_back(RegisterOperation::create_set_operation(
                                    reg_id,
                                    tag_op.is_multi_valued()
                            ));
                        } else if (TagOperationType::Negate == tag_op.get_type()) {
                            reg_ops.emplace_back(RegisterOperation::create_negate_operation(
                                    reg_id,
                                    tag_op.is_multi_valued()
                            ));
                        }
                    } else {
                        if (TagOperationType::Set == tag_op.get_type()) {
                            reg_ops.emplace_back(RegisterOperation::create_set_operation(
                                    reg_id,
                                    tag_op.is_multi_valued()
                            ));
                        } else if (TagOperationType::Negate == tag_op.get_type()) {
                            reg_ops.emplace_back(RegisterOperation::create_negate_operation(
                                    reg_id,
                                    tag_op.is_multi_valued()
                            ));
                        }
                    }
                }
                config.set_reg_id(tag_id, tag_id_with_op_to_reg_id.at(tag_id));
            }
        }
        new_closure.insert(config);
    }
    closure = new_closure;
    return reg_ops;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::reassign_transition_reg_ops(
        std::unordered_map<reg_id_t, reg_id_t> const& reg_map,
        std::vector<RegisterOperation>& reg_ops
) -> void {
    for (auto const [old_reg_id, new_reg_id] : reg_map) {
        if (old_reg_id == new_reg_id) {
            continue;
        }
        bool is_existing_reg_op_mapping{false};
        bool multi_valued;
        for (auto& reg_op : reg_ops) {
            if (reg_op.get_reg_id() == old_reg_id) {
                reg_op.set_reg_id(new_reg_id);
                is_existing_reg_op_mapping = true;
                multi_valued = reg_op.is_multi_valued();
                break;
            }
        }
        if (false == is_existing_reg_op_mapping) {
            reg_ops.emplace_back(
                    RegisterOperation::create_copy_operation(new_reg_id, old_reg_id, multi_valued)
            );
        }
    }
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::new_state(
        ConfigurationSet const& config_set,
        std::map<tag_id_t, reg_id_t> const& tag_id_to_final_reg_id,
        std::vector<bool> const multi_valued
) -> TypedDfaState* {
    m_states.emplace_back(std::make_unique<TypedDfaState>());
    auto* dfa_state = m_states.back().get();
    for (auto const& config : config_set) {
        auto const* nfa_state = config.get_state();
        if (nfa_state->is_accepting()) {
            dfa_state->add_matching_variable_id(nfa_state->get_matching_variable_id());
            for (auto const [tag_id, final_reg_id] : tag_id_to_final_reg_id) {
                auto const optional_tag_op{config.get_tag_lookahead(tag_id)};
                if (optional_tag_op.has_value()) {
                    auto const tag_op{optional_tag_op.value()};
                    if (TagOperationType::Set == tag_op.get_type()) {
                        dfa_state->add_accepting_op(RegisterOperation::create_set_operation(
                                final_reg_id,
                                tag_op.is_multi_valued()
                        ));
                    } else if (TagOperationType::Negate == tag_op.get_type()) {
                        dfa_state->add_accepting_op(RegisterOperation::create_negate_operation(
                                final_reg_id,
                                tag_op.is_multi_valued()
                        ));
                    }
                } else {
                    // Note: `config` must have a reg for this tag so we just call `at`.
                    auto const prev_reg_id{config.get_tag_id_to_reg_ids().at(tag_id)};
                    dfa_state->add_accepting_op(RegisterOperation::create_copy_operation(
                            final_reg_id,
                            prev_reg_id,
                            multi_valued.at(tag_id)
                    ));
                }
            }
        }
    }
    return dfa_state;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::get_intersect(Dfa const* dfa_in
) const -> std::set<uint32_t> {
    std::set<uint32_t> schema_types;
    std::set<DfaStatePair<TypedDfaState>> unvisited_pairs;
    std::set<DfaStatePair<TypedDfaState>> visited_pairs;
    unvisited_pairs.emplace(get_root(), dfa_in->get_root());
    // TODO: Handle UTF-8 (multi-byte transitions) as well
    while (false == unvisited_pairs.empty()) {
        auto current_pair_it = unvisited_pairs.begin();
        if (current_pair_it->is_accepting()) {
            auto const& matching_variable_ids = current_pair_it->get_matching_variable_ids();
            schema_types.insert(matching_variable_ids.cbegin(), matching_variable_ids.cend());
        }
        visited_pairs.insert(*current_pair_it);
        current_pair_it->get_reachable_pairs(visited_pairs, unvisited_pairs);
        unvisited_pairs.erase(current_pair_it);
    }
    return schema_types;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::get_bfs_traversal_order(
) const -> std::vector<TypedDfaState const*> {
    std::queue<TypedDfaState const*> state_queue;
    std::unordered_set<TypedDfaState const*> visited_states;
    std::vector<TypedDfaState const*> visited_order;
    visited_states.reserve(m_states.size());
    visited_order.reserve(m_states.size());

    auto try_add_to_queue_and_visited
            = [&state_queue, &visited_states](TypedDfaState const* dest_state) {
                  if (visited_states.insert(dest_state).second) {
                      state_queue.push(dest_state);
                  }
              };

    try_add_to_queue_and_visited(get_root());
    while (false == state_queue.empty()) {
        auto const* current_state = state_queue.front();
        visited_order.push_back(current_state);
        state_queue.pop();
        // TODO: Handle the utf8 case
        for (uint32_t idx{0}; idx < cSizeOfByte; ++idx) {
            auto const& transition{current_state->get_transition(idx)};
            if (transition.has_value()) {
                auto const* dest_state{transition->get_dest_state()};
                try_add_to_queue_and_visited(dest_state);
            }
        }
    }
    return visited_order;
}

template <typename TypedDfaState, typename TypedNfaState>
auto Dfa<TypedDfaState, TypedNfaState>::serialize() const -> std::optional<std::string> {
    auto const traversal_order = get_bfs_traversal_order();

    std::unordered_map<TypedDfaState const*, uint32_t> state_ids;
    state_ids.reserve(traversal_order.size());
    for (auto const* state : traversal_order) {
        state_ids.emplace(state, state_ids.size());
    }

    std::vector<std::string> serialized_states;
    for (auto const* state : traversal_order) {
        auto const optional_serialized_state{state->serialize(state_ids)};
        if (false == optional_serialized_state.has_value()) {
            return std::nullopt;
        }
        serialized_states.emplace_back(optional_serialized_state.value());
    }
    return fmt::format("{}\n", fmt::join(serialized_states, "\n"));
}
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_DFA_HPP
