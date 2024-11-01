#ifndef LOG_SURGEON_FINITE_AUTOMATA_TAGGED_TRANSITION
#define LOG_SURGEON_FINITE_AUTOMATA_TAGGED_TRANSITION

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include <log_surgeon/finite_automata/Tag.hpp>

namespace log_surgeon::finite_automata {
template <typename NFAStateType>
class PositiveTaggedTransition {
public:
    PositiveTaggedTransition(Tag* tag, NFAStateType const* dest_state)
            : m_tag{tag},
              m_dest_state{dest_state} {}

    [[nodiscard]] auto get_dest_state() const -> NFAStateType const* { return m_dest_state; }

    auto add_tag_start_pos(uint32_t start_pos) const -> void { m_tag->add_start_pos(start_pos); }

    auto add_tag_end_pos(uint32_t end_pos) const -> void { m_tag->add_end_pos(end_pos); }

    /**
     * @param state_ids A map of states to their unique identifiers.
     * @return A string representation of the positive tagged transition on success.
     * @return std::nullopt if `m_dest_state` is not in `state_ids`.
     */
    [[nodiscard]] auto serialize(std::unordered_map<NFAStateType const*, uint32_t> const& state_ids
    ) const -> std::optional<std::string>;

private:
    Tag* m_tag;
    NFAStateType const* m_dest_state;
};

template <typename NFAStateType>
class NegativeTaggedTransition {
public:
    NegativeTaggedTransition(std::vector<Tag*> tags, NFAStateType const* dest_state)
            : m_tags{std::move(tags)},
              m_dest_state{dest_state} {}

    [[nodiscard]] auto get_dest_state() const -> NFAStateType const* { return m_dest_state; }

    auto negate_tag() const -> void {
        for (auto* tag : m_tags) {
            tag->set_unmatched();
        }
    }

    /**
     * @param state_ids A map of states to their unique identifiers.
     * @return A string representation of the negative tagged transition on success.
     * @return std::nullopt if `m_dest_state` is not in `state_ids`.
     */
    [[nodiscard]] auto serialize(std::unordered_map<NFAStateType const*, uint32_t> const& state_ids
    ) const -> std::optional<std::string>;

private:
    std::vector<Tag*> const m_tags;
    NFAStateType const* m_dest_state;
};

template <typename NFAStateType>
auto PositiveTaggedTransition<NFAStateType>::serialize(
        std::unordered_map<NFAStateType const*, uint32_t> const& state_ids
) const -> std::optional<std::string> {
    auto const state_id_it = state_ids.find(m_dest_state);
    if (state_id_it == state_ids.end()) {
        return std::nullopt;
    }
    return fmt::format("{}[{}]", state_id_it->second, m_tag->get_name());
}

template <typename NFAStateType>
auto NegativeTaggedTransition<NFAStateType>::serialize(
        std::unordered_map<NFAStateType const*, uint32_t> const& state_ids
) const -> std::optional<std::string> {
    auto const state_id_it = state_ids.find(m_dest_state);
    if (state_id_it == state_ids.end()) {
        return std::nullopt;
    }

    auto const tag_names = m_tags | std::ranges::views::transform([](Tag const* tag) {
                               return tag->get_name();
                           });
    return fmt::format("{}[{}]", state_id_it->second, fmt::join(tag_names, ","));
}
}  // namespace log_surgeon::finite_automata

#endif  // LOG_SURGEON_FINITE_AUTOMATA_TAGGED_TRANSITION