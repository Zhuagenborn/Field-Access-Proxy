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

    std::uint16_t major_minor_verions {0x1234};
    String type {'t', 'y', 'p', 'e'};
    std::size_t opposite_endian_item_count {std::byteswap(max_items)};
    Item first_item[1];
};

//! Simulate a flexible array with additional elements.
struct PacketItems : Packet {
    Item remain_items[max_items - 1];
};

#pragma pack(pop)

namespace fm {

std::string FormatType(const String& type) noexcept {
    return std::string(type.cbegin(), type.cend());
}

std::string FormatVersion(const Packet& pkg, const std::uint8_t version) noexcept {
    return std::format("{}: v{}", FormatType(pkg.type), version);
}

std::string FormatItems(const FlexibleArray<Item>&) noexcept {
    return "{ items... }";
}

}  // namespace fm

namespace vt {

const auto type {MakeField("The type", &Packet::type,
                           [](const Packet&, const String& type) { return fm::FormatType(type); })};

const auto version {MakeField("The version", &Packet::major_minor_verions)};
const auto opposite_endian_item_count {
    MakeField("The number of items", &Packet::opposite_endian_item_count, GetOppositeEndian())};
const auto first_item {MakeField("The first item", &Packet::first_item)};

const auto major_version {MakeBitField("The major version", version, CHAR_BIT, CHAR_BIT)};
const auto minor_version {
    MakeBitField("The minor version", version, 0, CHAR_BIT, fm::FormatVersion)};

const auto is_version_first_bit_set {
    MakeBoolField("Whether the first bit of the version is set", version, 0)};

const auto flexible_items {MakeFlexibleArrayField(
    "Items", &Packet::first_item, opposite_endian_item_count, 0,
    [](const Packet&, const FlexibleArray<Item>& items) { return fm::FormatItems(items); })};

const auto fixed_flexible_items {
    MakeFlexibleArrayField("Items", &Packet::first_item, MakeConstant(Packet::max_items))};

}  // namespace vt

}  // namespace

TEST(CStyleFieldAccessProxy, Get) {
    const PacketItems pkg_items;
    const auto& pkg {static_cast<const Packet&>(pkg_items)};

    EXPECT_EQ(vt::opposite_endian_item_count.Get(pkg),
              std::byteswap(pkg.opposite_endian_item_count));
    EXPECT_EQ(vt::type.Get(pkg), pkg.type);
    EXPECT_EQ(vt::version.Get(pkg), pkg.major_minor_verions);
    EXPECT_EQ(vt::first_item.Get(pkg), pkg.first_item);
    EXPECT_EQ(vt::is_version_first_bit_set.Get(pkg), bit::IsBitSet(pkg.major_minor_verions, 0));

    EXPECT_EQ(vt::major_version.Get(pkg), bit::GetHighByte(pkg.major_minor_verions));
    EXPECT_EQ(vt::minor_version.Get(pkg), bit::GetLowByte(pkg.major_minor_verions));

    FlexibleArray<Item> items {pkg_items.first_item[0]};
    std::ranges::copy(pkg_items.remain_items, std::back_inserter(items));
    EXPECT_EQ(vt::flexible_items.GetAt(pkg, 0), items.front());
    EXPECT_EQ(vt::flexible_items.GetAll(pkg), items);

    EXPECT_EQ(vt::fixed_flexible_items.Get(pkg), items);
}

TEST(CStyleFieldAccessProxy, Set) {
    PacketItems pkg_items;
    auto& pkg {static_cast<Packet&>(pkg_items)};
    {
        pkg.opposite_endian_item_count = 0;
        vt::opposite_endian_item_count.Set(pkg, Packet::max_items);
        EXPECT_EQ(std::byteswap(pkg.opposite_endian_item_count), Packet::max_items);
    }
    {
        constexpr String type {'t', 'e', 's', 't'};
        pkg.type = {};
        vt::type.Set(pkg, type);
        EXPECT_EQ(pkg.type, type);
    }
    {
        constexpr std::uint8_t new_major_version {0xFF};
        constexpr std::uint8_t new_minor_version {0xAA};

        pkg.major_minor_verions = 0;
        vt::major_version.Set(pkg, new_major_version);
        vt::minor_version.Set(pkg, new_minor_version);
        EXPECT_EQ(pkg.major_minor_verions, bit::CombineBytes(new_major_version, new_minor_version));
    }
    {
        constexpr Item new_item {'t', 'e', 's', 't'};
        const FlexibleArray<Item> new_items(3, new_item);
        ASSERT_NE(new_items.size(), Packet::max_items);

        vt::flexible_items.SetAt(pkg, 0, new_item);
        EXPECT_EQ(pkg_items.first_item[0], new_item);

        vt::flexible_items.SetAt(pkg, 1, new_item);
        EXPECT_EQ(pkg_items.remain_items[0], new_item);

        vt::flexible_items.SetAll(pkg, new_items, false);
        EXPECT_NE(std::byteswap(pkg.opposite_endian_item_count), new_items.size());

        vt::flexible_items.SetAll(pkg, new_items, true);
        EXPECT_EQ(std::byteswap(pkg.opposite_endian_item_count), new_items.size());

        const FlexibleArray<Item> read_items {pkg_items.first_item[0], pkg_items.remain_items[0],
                                              pkg_items.remain_items[1]};
        EXPECT_EQ(read_items, new_items);
    }
}

TEST(CStyleFieldAccessProxy, Format) {
    const PacketItems pkg_items;
    const auto& pkg {static_cast<const Packet&>(pkg_items)};
    {
        EXPECT_EQ(vt::opposite_endian_item_count.Format(pkg),
                  std::format("{}: {}", vt::opposite_endian_item_count.GetName(),
                              std::byteswap(pkg.opposite_endian_item_count)));
        EXPECT_EQ(vt::minor_version.Format(pkg),
                  fm::FormatVersion(pkg, vt::minor_version.Get(pkg)));
        EXPECT_EQ(vt::flexible_items.Format(pkg), fm::FormatItems(vt::flexible_items.Get(pkg)));
    }
    {
        std::stringstream target, formatted;
        target << std::format("{}: {}\n", vt::opposite_endian_item_count.GetName(),
                              std::byteswap(pkg.opposite_endian_item_count));
        target << fm::FormatVersion(pkg, vt::minor_version.Get(pkg)) << '\n';
        target << fm::FormatItems(vt::flexible_items.Get(pkg)) << '\n';

        const auto fields {
            std::make_tuple(vt::opposite_endian_item_count, vt::minor_version, vt::flexible_items)};
        PrintFields(formatted, pkg, fields);

        EXPECT_EQ(formatted.str(), target.str());
    }
}