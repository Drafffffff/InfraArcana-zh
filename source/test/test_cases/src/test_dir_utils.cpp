// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <string>

#include "catch.hpp"
#include "direction.hpp"
#include "pos.hpp"

using namespace dir_utils;

TEST_CASE("Compass direction name")
{
        const P p(20, 20);

        REQUIRE(compass_dir_name(p, p.with_x_offset(1)) == "东");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, 1)) == "东南");
        REQUIRE(compass_dir_name(p, p.with_y_offset(1)) == "南");
        REQUIRE(compass_dir_name(p, p.with_offsets(-1, 1)) == "西南");
        REQUIRE(compass_dir_name(p, p.with_x_offset(-1)) == "西");
        REQUIRE(compass_dir_name(p, p.with_offsets(-1, -1)) == "西北");
        REQUIRE(compass_dir_name(p, p.with_y_offset(-1)) == "北");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, -1)) == "东北");
        REQUIRE(compass_dir_name(p, p.with_offsets(3, 1)) == "东");
        REQUIRE(compass_dir_name(p, p.with_offsets(2, 3)) == "东南");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, 3)) == "南");
        REQUIRE(compass_dir_name(p, p.with_offsets(-3, 2)) == "西南");
        REQUIRE(compass_dir_name(p, p.with_offsets(-3, 1)) == "西");
        REQUIRE(compass_dir_name(p, p.with_offsets(-3, -2)) == "西北");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, -3)) == "北");
        REQUIRE(compass_dir_name(p, p.with_offsets(3, -2)) == "东北");
        REQUIRE(compass_dir_name(p, p.with_x_offset(10000)) == "东");
        REQUIRE(compass_dir_name(p, p.with_offsets(10000, 10000)) == "东南");
        REQUIRE(compass_dir_name(p, p.with_y_offset(10000)) == "南");
        REQUIRE(compass_dir_name(p, p.with_offsets(-10000, 10000)) == "西南");
        REQUIRE(compass_dir_name(p, p.with_x_offset(-10000)) == "西");
        REQUIRE(compass_dir_name(p, p.with_offsets(-10000, -10000)) == "西北");
        REQUIRE(compass_dir_name(p, p.with_y_offset(-10000)) == "北");
        REQUIRE(compass_dir_name(p, p.with_offsets(10000, -10000)) == "东北");
}
