#include "endian.h"
#include "field_access_proxy/field_access_proxy.h"

#include <bit_manip/bit_manip.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <tuple>

using namespace field_access_proxy;

namespace {

using String = std::array<char, 4>;

#pragma pack(push, 1)

struct Item {
    String msg {'1', '2', '3', '4'};

    constexpr bool operator==(const Item&) const noexcept = default;
};

struct Packet {
    static constexpr std::size_t max_items {10};
    static constexpr String default_type {'t', 'y', 'p', 'e'};
    static constexpr std::uint16_t default_major_minor_verions {0x1234};

    DEFINE_UNDERLYING_FIELD_WITH_PROXY(Packet, private, std::uint16_t, major_minor_verions,
                                       {default_major_minor_verions})
    DEFINE_FIELD_WITH_PROXY(Packet, String, public, Type, private, type, {default_type})
    DEFINE_INTEGRAL_FIELD_WITH_ENDIAN_PROXY(Packet, std::size_t, public, ItemCount, private,
                                            item_count, GetOppositeEndian(),
                                            {std::byteswap(max_items)})
    DEFINE_BIT_FIELD_WITH_PROXY(Packet, major_minor_verions, public, std::uint8_t, MajorVerion,
                                CHAR_BIT, CHAR_BIT)
    DEFINE_BIT_FIELD_WITH_PROXY(Packet, major_minor_verions, public, std::uint8_t, MinorVerion, 0,
                                CHAR_BIT)
    DEFINE_BOOL_FIELD_WITH_PROXY(Packet, major_minor_verions, public, IsFirstVersionBitSet,
                                 SetFirstVersionBit, first_version_bit, 0)
    DEFINE_FLEXIBLE_ARRAY_FIELD_WITH_PROXY(Packet, Item, public, Items, private, items, item_count,
                                           0)
};

//! Simulate a flexible array with additional elements.
struct PacketItems : Packet {
    Item remain_items[max_items - 1];
};

#pragma pack(pop)

}  // namespace

TEST(MacroDefinedFieldAccessProxy, Get) {
    const PacketItems pkg_items;
    const auto& pkg {static_cast<const Packet&>(pkg_items)};

    EXPECT_EQ(pkg.GetItemCount(), Packet::max_items);
    EXPECT_EQ(pkg.GetType(), Packet::default_type);

    EXPECT_EQ(pkg.GetMajorVerion(), bit::GetHighByte(Packet::default_major_minor_verions));
    EXPECT_EQ(pkg.GetMinorVerion(), bit::GetLowByte(Packet::default_major_minor_verions));
    EXPECT_EQ(pkg.IsFirstVersionBitSet(), bit::IsBitSet(Packet::default_major_minor_verions, 0));

    const FlexibleArray<Item> items(pkg.GetItemCount(), Item {});
    EXPECT_EQ(pkg.GetItems(), items);
}