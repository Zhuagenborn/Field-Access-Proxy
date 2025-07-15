# *C-Style* Field Access Proxy

![C++](docs/badges/C++.svg)
[![CMake](docs/badges/Made-with-CMake.svg)](https://cmake.org)
![GitHub Actions](docs/badges/Made-with-GitHub-Actions.svg)
![License](docs/badges/License-MIT.svg)

## Introduction

A header-only library written in *C++23* for accessing and formatting fields within *C-style* structures in a flexible and reusable way, supporting:

- Accessing and modifying regular fields, bit fields, and flexible arrays.
- Formatting fields as strings (optionally using custom formatters).
- Grouping fields together and print them in a structured format.

## Unit Tests

### Prerequisites

- Install *GoogleTest*.
- Install *CMake*.

### Building

Go to the project folder and run:

```bash
mkdir -p build
cd build
cmake -DFIELD_ACCESS_PROXY_BUILD_TESTS=ON ..
cmake --build .
```

### Running

Go to the `build` folder and run:

```bash
ctest -VV
```

## Examples

### Accessing Existing C-Style Structures

Suppose we have C-style structures.

```c++
using String = std::array<char, 4>;

struct Item {
    String msg;
};

struct Packet {
    std::uint16_t major_minor_verions;
    String type;
    std::size_t item_count;
    Item first_item[1];
};

// Simulate a flexible array with additional elements.
struct PacketItems : Packet {
    Item remain_items[9];
};
```

First, we need to create field proxies.

```c++
namespace vt {

// Regular fields.
const auto version {MakeField("The version", &Packet::major_minor_verions)};
const auto item_count {MakeField("The number of items", &Packet::item_count)};

// Bit fields.
const auto major_version {MakeBitField("The major version", version, CHAR_BIT, CHAR_BIT)};
const auto minor_version {MakeBitField("The minor version", version, 0, CHAR_BIT)};

// Flexible array fields.
const auto flexible_items {MakeFlexibleArrayField("Items", &Packet::first_item, item_count)};

}  // namespace vt
```

Then we can access fields in objects with proxies.

```c++
PacketItems pkg_items;
auto& pkg {static_cast<Packet&>(pkg_items)};
```

See more examples in `tests/c_style_tests.cpp`.

#### Retrieving Values

- Regular fields.

    ```c++
    EXPECT_EQ(vt::item_count.Get(pkg), pkg.item_count);
    EXPECT_EQ(vt::version.Get(pkg), pkg.major_minor_verions);
    ```

- Bit fields.

    ```c++
    EXPECT_EQ(vt::major_version.Get(pkg), bit::GetHighByte(pkg.major_minor_verions));
    EXPECT_EQ(vt::minor_version.Get(pkg), bit::GetLowByte(pkg.major_minor_verions));
    ```

- Flexible array fields.

    ```c++
    FlexibleArray<Item> items {pkg_items.first_item[0]};
    std::ranges::copy(pkg_items.remain_items, std::back_inserter(items));
    EXPECT_EQ(vt::flexible_items.GetAt(pkg, 0), items.front());
    EXPECT_EQ(vt::flexible_items.GetAll(pkg), items);
    ```

#### Modifying Values

- Regular fields.

    ```c++
    vt::item_count.Set(pkg, 2);
    EXPECT_EQ(pkg.item_count, 2);
    ```

- Bit fields.

    ```c++
    constexpr std::uint8_t new_major_version {0xFF};
    constexpr std::uint8_t new_minor_version {0xAA};
    vt::major_version.Set(pkg, new_major_version);
    vt::minor_version.Set(pkg, new_minor_version);
    EXPECT_EQ(pkg.major_minor_verions, bit::CombineBytes(new_major_version, new_minor_version));
    ```

- Flexible array fields.

    ```c++
    constexpr Item new_item {'t', 'e', 's', 't'};
    const FlexibleArray<Item> new_items(3, new_item);

    // Modifying the first two elements.
    vt::flexible_items.SetAt(pkg, 0, new_item);
    EXPECT_EQ(pkg_items.first_item[0], new_item);

    vt::flexible_items.SetAt(pkg, 1, new_item);
    EXPECT_EQ(pkg_items.remain_items[0], new_item);

    // Modifying all elements and update the count field.
    vt::flexible_items.SetAll(pkg, new_items, true);
    EXPECT_EQ(pkg.item_count, new_items.size());

    const FlexibleArray<Item> read_items {pkg_items.first_item[0], pkg_items.remain_items[0], pkg_items.remain_items[1]};
    EXPECT_EQ(read_items, new_items);
    ```

#### Formatting Values

When creating proxies, we can provide an optional callable method for custom formatting. Otherwise, `std::format` is used as default formatting `{name}: {value}`.

```c++
std::string FormatItems(const Packet&, const FlexibleArray<Item>&) noexcept {
    return "{ items... }";
}

const auto flexible_items {MakeFlexibleArrayField("Items", &Packet::first_item, item_count, 0, FormatItems)};
```

### Defining New Structures with Proxies

We can also define new structures directly with getters and setters.

```c++
struct Packet {
    DEFINE_UNDERLYING_FIELD_WITH_PROXY(Packet, private, std::uint16_t, major_minor_verions, {0x1234})
    DEFINE_FIELD_WITH_PROXY(Packet, std::size_t, public, ByteCount, private, byte_count, {10})
    DEFINE_BIT_FIELD_WITH_PROXY(Packet, major_minor_verions, public, std::uint8_t, MajorVerion, CHAR_BIT, CHAR_BIT)
    DEFINE_BIT_FIELD_WITH_PROXY(Packet, major_minor_verions, public, std::uint8_t, MinorVerion, 0, CHAR_BIT)
    DEFINE_BOOL_FIELD_WITH_PROXY(Packet, major_minor_verions, public, IsFirstVersionBitSet, SetFirstVersionBit, first_version_bit, 0)
    DEFINE_FLEXIBLE_ARRAY_FIELD_WITH_PROXY(Packet, std::byte, public, Bytes, private, bytes, byte_count, 0)
};
```

See more examples in `tests/macro_defined_tests.cpp`.

## License

Distributed under the *MIT License*. See `LICENSE` for more information.