#include <cstddef>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <log_surgeon/finite_automata/PrefixTree.hpp>
#include <log_surgeon/finite_automata/RegisterHandler.hpp>

using log_surgeon::finite_automata::RegisterHandler;
using position_t = log_surgeon::reg_pos_t;

namespace {
/**
 * @param multi_valued_list A list specifying if each registers to be added is multi-valued.
 * @return The newly initialized register handler.
 */
[[nodiscard]] auto handler_init(std::vector<bool> const& multi_valued_list) -> RegisterHandler;

auto handler_init(std::vector<bool> const& multi_valued_list) -> RegisterHandler {
    RegisterHandler handler;
    handler.add_registers(multi_valued_list);
    return handler;
}
}  // namespace

TEST_CASE("`RegisterHandler` tests", "[RegisterHandler]") {
    constexpr size_t cRegId1{0};
    constexpr size_t cRegId2{1};
    constexpr size_t cRegId3{2};
    constexpr size_t cRegId4{3};
    constexpr position_t cAppendPos1{5};
    constexpr position_t cAppendPos2{10};
    constexpr position_t cAppendPos3{15};
    constexpr position_t cSetPos1{6};
    constexpr position_t cSetPos2{11};
    std::vector const multi_valued{true, true, false, false};

    RegisterHandler handler{handler_init(multi_valued)};

    SECTION("Throws out of range correctly") {
        constexpr size_t cInvalidRegId{10};
        RegisterHandler empty_handler{handler_init({})};

        // Test empty handler throws
        REQUIRE_THROWS_AS(empty_handler.get_reversed_positions(cRegId1), std::out_of_range);
        REQUIRE_THROWS_AS(
                empty_handler.copy_single_valued_register(cRegId2, cRegId1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.copy_multi_valued_register(cRegId2, cRegId1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.set_single_valued_position(cRegId1, cAppendPos1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.append_multi_valued_position(cRegId1, cAppendPos1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(empty_handler.get_reversed_positions(cRegId1), std::out_of_range);

        // Test initialized handler throws
        REQUIRE_THROWS_AS(handler.get_reversed_positions(cInvalidRegId), std::out_of_range);
        REQUIRE_THROWS_AS(
                empty_handler.copy_single_valued_register(cInvalidRegId, cRegId1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.copy_multi_valued_register(cInvalidRegId, cRegId1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.copy_single_valued_register(cRegId1, cInvalidRegId),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.copy_multi_valued_register(cRegId1, cInvalidRegId),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.set_single_valued_position(cInvalidRegId, cAppendPos1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(
                empty_handler.append_multi_valued_position(cInvalidRegId, cAppendPos1),
                std::out_of_range
        );
        REQUIRE_THROWS_AS(empty_handler.get_reversed_positions(cInvalidRegId), std::out_of_range);
    }

    SECTION("Initial multi-valued register is empty") {
        auto positions{handler.get_reversed_positions(cRegId1)};
        REQUIRE(handler.get_reversed_positions(cRegId1).empty());

        handler.copy_multi_valued_register(cRegId2, cRegId1);
        REQUIRE(handler.get_reversed_positions(cRegId2).empty());
    }

    SECTION("Append, set, and copy position work correctly") {
        handler.append_multi_valued_position(cRegId1, cAppendPos1);
        handler.append_multi_valued_position(cRegId1, cAppendPos2);
        handler.append_multi_valued_position(cRegId1, cAppendPos3);
        REQUIRE(std::vector<position_t>{cAppendPos3, cAppendPos2, cAppendPos1}
                == handler.get_reversed_positions(cRegId1));

        handler.set_single_valued_position(cRegId3, cSetPos1);
        REQUIRE(std::vector<position_t>{cSetPos1} == handler.get_reversed_positions(cRegId3));

        handler.set_single_valued_position(cRegId3, cSetPos2);
        REQUIRE(std::vector<position_t>{cSetPos2} == handler.get_reversed_positions(cRegId3));

        handler.copy_multi_valued_register(cRegId2, cRegId1);
        REQUIRE(std::vector<position_t>{cAppendPos3, cAppendPos2, cAppendPos1}
                == handler.get_reversed_positions(cRegId2));

        handler.copy_single_valued_register(cRegId4, cRegId3);
        REQUIRE(std::vector<position_t>{cSetPos2} == handler.get_reversed_positions(cRegId4));
    }

    SECTION("Handles negative position values correctly") {
        constexpr position_t cNegativePos1{-1};
        constexpr position_t cNegativePos2{-100};

        handler.append_multi_valued_position(cRegId1, cNegativePos1);
        handler.append_multi_valued_position(cRegId1, cNegativePos2);
        REQUIRE(std::vector<position_t>{cNegativePos2, cNegativePos1}
                == handler.get_reversed_positions(cRegId1));

        handler.set_single_valued_position(cRegId3, cNegativePos1);
        handler.set_single_valued_position(cRegId3, cNegativePos2);
        REQUIRE(std::vector<position_t>{cNegativePos2} == handler.get_reversed_positions(cRegId3));
    }
}
