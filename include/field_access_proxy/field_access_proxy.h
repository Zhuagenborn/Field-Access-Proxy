/**
 * @file field_access_proxy.h
 * @brief
 * A header-only library for accessing and formatting fields within C-style structures in a flexible and reusable way.
 *
 * @details
 * It supports:
 * - Accessing and modifying regular fields, bit fields, and flexible arrays.
 * - Formatting fields as strings (optionally using custom formatters).
 * - Grouping fields together and print them in a structured format.
 *
 * @par GitHub
 * https://github.com/Zhuagenborn
 *
 * @date 2025-04-30
 *
 * @example tests/c_style_tests.cpp
 * @example tests/macro_defined_tests.cpp
 */

#pragma once

#include <cassert>
#include <climits>
#include <concepts>
#include <cstddef>
#include <format>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <bit_manip/bit_manip.h>

namespace field_access_proxy {

namespace impl {

//! Whether a type can be formatted using @p std::formatter.
template <typename T, typename Char = char>
concept IsFormattable = requires(
    const T val,
    std::basic_format_context<std::back_insert_iterator<std::basic_string<Char>>, Char>& ctx) {
    { std::formatter<std::decay_t<T>, Char> {}.format(val, ctx) };
};

//! A mixin class that provides a name to derived classes.
class Named {
public:
    constexpr explicit Named(std::string name) noexcept : name_ {std::move(name)} {
        assert(!name_.empty());
    }

    constexpr std::string_view GetName() const noexcept {
        return name_;
    }

private:
    std::string name_;
};

/**
 * @brief A mixin class that enables formatting of a field within a structure.
 *
 * @details
 * This class supports custom or default formatting of a field extracted via an adapter,
 * and optionally applies a user-defined formatter.
 *
 * @tparam Struct The type of the structure containing the field.
 * @tparam FieldProxy The field proxy (e.g., @p Field) that provides access to its value and name.
 * @tparam RawField The raw type of the field to be formatted (e.g., @p int).
 * @tparam Formatter An optional callable type for custom formatting.
 */
template <typename Struct, typename FieldProxy, typename RawField,
          typename Formatter = std::nullptr_t>
class Formattable {
public:
    explicit Formattable(Formatter&& formatter = nullptr) noexcept :
        formatter_ {std::forward<Formatter>(formatter)} {}

    /**
     * @brief Format the value of a field within a structure to a string.
     *
     * @details
     * If a custom formatter is provided, it is used to format the field value.
     * Otherwise, the method falls back to default formatting using @p std::format.
     *
     * If neither a custom formatter is supplied nor the field type is formattable via @p std::format,
     * the method triggers an assertion failure and reaches an unreachable state.
     */
    std::string Format(const Struct& obj) const {
        const auto& field {static_cast<const FieldProxy&>(*this)};
        const auto& val {field.Get(obj)};
        if constexpr (!std::same_as<std::decay_t<Formatter>, std::nullptr_t>) {
            return formatter_(obj, val);
        } else {
            if constexpr (IsFormattable<RawField>) {
                return std::format("{}: {}", field.GetName(), val);
            } else {
                assert(false);
                std::unreachable();
            }
        }
    }

private:
    Formatter formatter_;
};

}  // namespace impl

/**
 * @brief A regular field proxy in a structure.
 *
 * @tparam Struct_ The structure type.
 * @tparam RawField The raw field type (e.g., @p std::uint32_t).
 * @tparam Formatter An optional callable for custom formatting.
 */
template <typename Struct_, typename RawField, typename Formatter = std::nullptr_t>
class Field :
    public impl::Named,
    public impl::Formattable<Struct_, Field<Struct_, RawField, Formatter>, RawField, Formatter> {
public:
    using Struct = Struct_;
    using Value = RawField;

    /**
     * @brief Create a new field proxy.
     *
     * @param name The field name.
     * @param field A pointer-to-member specifying the field within the structure.
     * @param formatter An optional formatter used for field formatting.
     */
    explicit Field(std::string name, Value Struct::* const field,
                   Formatter&& formatter = nullptr) noexcept :
        Named {std::move(name)},
        impl::Formattable<Struct, Field<Struct, Value, Formatter>, Value, Formatter> {
            std::forward<Formatter>(formatter)},
        field_ {field} {
        assert(field_ != nullptr);
    }

    /**
     * @brief Create a new integral field proxy with endian support.
     *
     * @param name The field name.
     * @param field A pointer-to-member specifying the field within the structure.
     * @param endian The endianness of the field.
     * @param formatter An optional formatter used for field formatting.
     */
    explicit Field(std::string name, Value Struct::* const field, const std::endian endian,
                   Formatter&& formatter = nullptr) noexcept
        requires std::is_integral_v<Value>
        :
        Named {std::move(name)},
        impl::Formattable<Struct, Field<Struct, Value, Formatter>, Value, Formatter> {
            std::forward<Formatter>(formatter)},
        field_ {field},
        endian_ {endian} {
        assert(field_ != nullptr);
    }

    //! Get the value of the field from an object.
    auto Get(const Struct& obj) const noexcept {
        if constexpr (std::is_integral_v<Value>) {
            return endian_ == std::endian::native ? obj.*field_ : std::byteswap(obj.*field_);
        } else {
            return static_cast<const Value&>(obj.*field_);
        }
    }

    //! Set the field to a new value for an object.
    const Field& Set(Struct& obj, Value val) const noexcept {
        if constexpr (std::is_integral_v<Value>) {
            obj.*field_ = endian_ == std::endian::native ? std::move(val) : std::byteswap(val);
        } else {
            obj.*field_ = std::move(val);
        }
        return *this;
    }

private:
    std::endian endian_ {std::endian::native};
    Value Struct::* field_;
};

/**
 * @brief A bit field proxy within a parent integral field of a structure.
 *
 * @tparam ParentFieldProxy The parent field proxy (e.g., @p Field) to access the integral field that contains this bit field.
 * @tparam Target The type of the bit field (e.g., @p std::uint8_t).
 * @tparam Formatter An optional callable for custom formatting.
 */
template <typename ParentFieldProxy, typename Target, typename Formatter = std::nullptr_t>
    requires std::is_integral_v<Target> || std::same_as<Target, std::byte>
class BitField :
    public impl::Named,
    public impl::Formattable<typename ParentFieldProxy::Struct,
                             BitField<ParentFieldProxy, Target, Formatter>, Target, Formatter> {
public:
    using Struct = typename ParentFieldProxy::Struct;
    using Value = Target;

    /**
     * @brief Create a new bit field proxy.
     *
     * @param name The field name.
     * @param parent The parent field proxy that provides access to the underlying integral field.
     * @param bit_offset The offset in bits from the least significant bit of the parent field.
     * @param bit_width The width of the bit field in bits.
     * @param formatter An optional formatter used for field formatting.
     */
    explicit BitField(std::string name, ParentFieldProxy parent, const std::size_t bit_offset,
                      const std::size_t bit_width, Formatter&& formatter = nullptr) noexcept
        requires(!std::same_as<Value, bool>)
        :
        Named {std::move(name)},
        impl::Formattable<Struct, BitField<ParentFieldProxy, Value, Formatter>, Value, Formatter> {
            std::forward<Formatter>(formatter)},
        parent_ {std::move(parent)},
        bit_offset_ {bit_offset},
        bit_width_ {bit_width} {}

    /**
     * @brief Create a new boolean field proxy.
     *
     * @param name The field name.
     * @param parent The parent field proxy that provides access to the underlying integral field.
     * @param bit_pos The offset in bits from the least significant bit of the parent field.
     * @param formatter An optional formatter used for field formatting.
     */
    explicit BitField(std::string name, ParentFieldProxy parent, const std::size_t bit_pos,
                      Formatter&& formatter = nullptr) noexcept
        requires std::same_as<Value, bool>
        :
        Named {std::move(name)},
        impl::Formattable<Struct, BitField<ParentFieldProxy, Value, Formatter>, Value, Formatter> {
            std::forward<Formatter>(formatter)},
        parent_ {std::move(parent)},
        bit_offset_ {bit_pos},
        bit_width_ {1} {}

    //! Get the value of the field from an object.
    Value Get(const Struct& obj) const noexcept {
        const auto field {parent_.Get(obj)};
        if constexpr (std::same_as<Value, bool>) {
            return bit::IsBitSet(field, bit_offset_);
        } else {
            return static_cast<Value>(bit::GetBits(field, bit_offset_, bit_width_));
        }
    }

    //! Set the field to a new value for an object.
    const BitField& Set(Struct& obj, const Value val) const noexcept {
        auto field {parent_.Get(obj)};
        if constexpr (std::same_as<Value, bool>) {
            if (val) {
                bit::SetBit(field, bit_offset_);
            } else {
                bit::ClearBit(field, bit_offset_);
            }
        } else {
            bit::SetBits(field, val, bit_offset_, bit_width_);
        }

        parent_.Set(obj, field);
        return *this;
    }

private:
    ParentFieldProxy parent_;
    std::size_t bit_offset_;
    std::size_t bit_width_;
};

//! A boolean field proxy within a parent integral field of a structure.
template <typename ParentFieldProxy, typename Formatter = std::nullptr_t>
using BoolField = BitField<ParentFieldProxy, bool, Formatter>;

template <typename T>
using FlexibleArray = std::vector<T>;

/**
 * @brief A flexible array field proxy within a structure where the element count is specified by another field.
 *
 * @tparam Struct_ The structure type.
 * @tparam Array The flexible array type (e.g., @p int[1]).
 * @tparam CountFieldProxy A field proxy (e.g., @p Field) to access the count of valid elements.
 * @tparam Formatter An optional callable for custom formatting.
 */
template <typename Struct_, typename Array, typename CountFieldProxy,
          typename Formatter = std::nullptr_t>
class FlexibleArrayField :
    public impl::Named,
    public impl::Formattable<Struct_,
                             FlexibleArrayField<Struct_, Array, CountFieldProxy, Formatter>,
                             FlexibleArray<std::remove_extent_t<Array>>, Formatter> {
public:
    using Struct = Struct_;
    using Element = std::remove_extent_t<Array>;
    using Value = FlexibleArray<Element>;

    /**
     * @brief Create a new proxy for a flexible array field.
     *
     * @param name The field name.
     * @param array A pointer-to-member representing the flexible array within the structure.
     * @param count A field proxy that specifies the number of valid elements in the array.
     * @param formatter An optional formatter used for field formatting.
     */
    explicit FlexibleArrayField(std::string name, Array Struct::* const array,
                                const CountFieldProxy count,
                                Formatter&& formatter = nullptr) noexcept :
        Named {std::move(name)},
        impl::Formattable<Struct, FlexibleArrayField<Struct, Array, CountFieldProxy, Formatter>,
                          FlexibleArray<Element>, Formatter> {std::forward<Formatter>(formatter)},
        count_ {count},
        array_ {array} {
        assert(array_ != nullptr);
    }

    //! Get all elements of the field using the count field from an object.
    Value GetAll(const Struct& obj) const noexcept {
        const auto count {count_.Get(obj)};
        const auto base {GetAddr(obj)};
        return Value(base, base + count);
    }

    //! Get an element at the specified position of the field from an object.
    const Element& GetAt(const Struct& obj, const std::size_t pos) const noexcept {
        assert(pos < count_.Get(obj));
        const auto base {GetAddr(obj)};
        return *(base + pos);
    }

    //! Set all elements of the field to new values and optionally updates the count field for an object.
    const FlexibleArrayField& SetAll(Struct& obj, const Value& vals,
                                     const bool update_count = true) const noexcept {
        const auto base {GetAddr(obj)};
        std::ranges::copy(vals, base);
        if (update_count) {
            count_.Set(obj, vals.size());
        }

        return *this;
    }

    //! Set an element at the specified position of the field to a new value for an object.
    const FlexibleArrayField& SetAt(Struct& obj, const std::size_t pos,
                                    const Element& elem) const noexcept {
        assert(pos < count_.Get(obj));
        const auto base {GetAddr(obj)};
        *(base + pos) = elem;
        return *this;
    }

    //! Same as @ref GetAll without updating the count field.
    Value Get(const Struct& obj) const noexcept {
        return GetAll(obj);
    }

    //! Same as @ref SetAll.
    const FlexibleArrayField& Set(Struct& obj, const Value& vals) const noexcept {
        return SetAll(obj, vals);
    }

private:
    const Element* GetAddr(const Struct& obj) const noexcept {
        return std::addressof((obj.*array_)[0]);
    }

    Element* GetAddr(Struct& obj) const noexcept {
        return const_cast<Element*>(GetAddr(const_cast<const Struct&>(obj)));
    }

    CountFieldProxy count_;
    Array Struct::* array_;
};

//! A constant value wrapper to allow proxy-like reading.
template <typename T>
class Constant {
public:
    explicit Constant(T val) noexcept : val_ {std::move(val)} {}

    //! Always return the stored constant value.
    const T& Get(const auto&) const noexcept {
        return val_;
    }

    //! Intentionally does nothing.
    void Set(const auto&, const T&) const noexcept {}

private:
    T val_;
};

//! Make a regular field proxy in a structure.
template <typename Struct, typename RawField, typename Formatter = std::nullptr_t>
auto MakeField(std::string name, RawField Struct::* const field,
               Formatter&& formatter = nullptr) noexcept {
    return Field<Struct, RawField, Formatter> {std::move(name), field,
                                               std::forward<Formatter>(formatter)};
}

//! @overload
template <typename Struct, typename RawField, typename Formatter = std::nullptr_t>
auto MakeField(std::string name, RawField Struct::* const field, const std::endian endian,
               Formatter&& formatter = nullptr) noexcept {
    return Field<Struct, RawField, Formatter> {std::move(name), field, endian,
                                               std::forward<Formatter>(formatter)};
}

//! Make a bit field proxy within a parent integral field of a structure.
template <typename ParentFieldProxy, typename Target = typename ParentFieldProxy::Value,
          typename Formatter = std::nullptr_t>
    requires(!std::same_as<Target, bool>)
auto MakeBitField(std::string name, const ParentFieldProxy parent, const std::size_t offset,
                  const std::size_t width, Formatter&& formatter = nullptr) noexcept {
    return BitField<ParentFieldProxy, Target, Formatter> {std::move(name), parent, offset, width,
                                                          std::forward<Formatter>(formatter)};
}

//! Make a boolean field proxy within a parent integral field of a structure.
template <typename ParentFieldProxy, typename Formatter = std::nullptr_t>
auto MakeBoolField(std::string name, const ParentFieldProxy parent, const std::size_t bit_pos,
                   Formatter&& formatter = nullptr) noexcept {
    return BoolField<ParentFieldProxy, Formatter> {std::move(name), parent, bit_pos,
                                                   std::forward<Formatter>(formatter)};
}

//! Make a flexible array field proxy within a structure where the element count is specified by another field.
template <typename Struct, typename Array, typename CountFieldProxy,
          typename Formatter = std::nullptr_t>
auto MakeFlexibleArrayField(std::string name, Array Struct::* const array,
                            const CountFieldProxy count, Formatter&& formatter = nullptr) noexcept {
    return FlexibleArrayField<Struct, Array, CountFieldProxy, Formatter> {
        std::move(name), array, count, std::forward<Formatter>(formatter)};
}

//! Make a constant value wrapper to allow proxy-like reading.
template <typename T>
auto MakeConstant(T val) noexcept {
    return Constant {std::move(val)};
}

//! Print all formatted fields from a tuple to the provided output stream.
template <typename Struct, typename... Fields>
std::ostream& PrintFields(std::ostream& os, const Struct& obj,
                          const std::tuple<Fields...>& fields) {
    std::apply([&obj, &os](const auto&... field) { ((os << field.Format(obj) << '\n'), ...); },
               fields);
    return os;
}

}  // namespace field_access_proxy

/**
 * @brief Define a private regular field with an associated proxy in a structure.
 *
 * @details
 * This macro declares the following elements within @p StructType,
 * typically used to define underlying integral fields for bit fields.
 * - A private member field @p field_name of type @p FieldType, optionally be initialized via @p field_init.
 * - A static private field proxy @p <field_name>_proxy of type @p Field.
 *
 * @param StructType The structure type.
 * @param FieldType The raw field type.
 * @param field_name The field name without quotes.
 * @param field_init An optional initializer for the field (e.g., @p {0}).
 */
#define DEFINE_PRIVATE_FIELD_WITH_PROXY(StructType, FieldType, field_name, field_init) \
private:                                                                               \
    FieldType field_name field_init;                                                   \
    inline static const auto field_name##_proxy {                                      \
        ::field_access_proxy::MakeField(#field_name, &StructType::field_name)};

//! @overload
#define DEFINE_PRIVATE_INTEGRAL_FIELD_WITH_ENDIAN_PROXY(StructType, FieldType, field_name, endian, \
                                                        field_init)                                \
private:                                                                                           \
    FieldType field_name field_init;                                                               \
    inline static const auto field_name##_proxy {                                                  \
        ::field_access_proxy::MakeField(#field_name, &StructType::field_name, endian)};

/**
 * @brief Define a regular field with an associated proxy and accessors in a structure.
 *
 * @details
 * This macro declares the following elements within @p StructType:
 * - Same as @ref DEFINE_PRIVATE_FIELD_WITH_PROXY.
 * - Public member methods @p Get<field_name> and @p Set<field_name>.
 */
#define DEFINE_FIELD_WITH_PROXY(StructType, FieldType, field_name, field_init) \
public:                                                                        \
    auto Get##field_name() const noexcept {                                    \
        return field_name##_proxy.Get(*this);                                  \
    }                                                                          \
                                                                               \
    StructType& Set##field_name(FieldType val) noexcept {                      \
        field_name##_proxy.Set(*this, std::move(val));                         \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    DEFINE_PRIVATE_FIELD_WITH_PROXY(StructType, FieldType, field_name, field_init)

//! @overload
#define DEFINE_INTEGRAL_FIELD_WITH_ENDIAN_PROXY(StructType, FieldType, field_name, endian,     \
                                                field_init)                                    \
public:                                                                                        \
    auto Get##field_name() const noexcept {                                                    \
        return field_name##_proxy.Get(*this);                                                  \
    }                                                                                          \
                                                                                               \
    StructType& Set##field_name(FieldType val) noexcept {                                      \
        field_name##_proxy.Set(*this, std::move(val));                                         \
        return *this;                                                                          \
    }                                                                                          \
                                                                                               \
    DEFINE_PRIVATE_INTEGRAL_FIELD_WITH_ENDIAN_PROXY(StructType, FieldType, field_name, endian, \
                                                    field_init)

/**
 * @brief Define a bit field with an associated proxy and accessors in a structure.
 *
 * @details
 * This macro declares the following elements within @p StructType:
 * - A static private bit field proxy @p <field_name>_proxy of type @p BitField.
 * - Public member methods @p Get<field_name> and @p Set<field_name>.
 *
 * @param StructType The structure type.
 * @param parent_field_name
 * The name (without quotes) of the existing integral field containing this bit field.
 * @p <parent_field_name>_proxy must have been defined within @p StructType.
 * @param FieldType The raw type of the bit field (e.g., @p std::uint8_t).
 * @param field_name The bit field name without quotes.
 * @param bit_offset The offset in bits from the least significant bit of the parent field.
 * @param bit_width The width of the bit field in bits.
 */
#define DEFINE_BIT_FIELD_WITH_PROXY(StructType, parent_field_name, FieldType, field_name, \
                                    bit_offset, bit_width)                                \
public:                                                                                   \
    auto Get##field_name() const noexcept {                                               \
        return field_name##_proxy.Get(*this);                                             \
    }                                                                                     \
                                                                                          \
    StructType& Set##field_name(const FieldType val) noexcept {                           \
        field_name##_proxy.Set(*this, val);                                               \
        return *this;                                                                     \
    }                                                                                     \
                                                                                          \
private:                                                                                  \
    inline static const auto field_name##_proxy {::field_access_proxy::MakeBitField(      \
        #field_name, parent_field_name##_proxy, bit_offset, bit_width)};

/**
 * @brief Define a boolean field with an associated proxy and accessors in a structure.
 *
 * @details
 * This macro declares the following elements within @p StructType:
 * - A static private boolean field proxy @p <field_name>_proxy of type @p BoolField.
 * - Public member methods @p getter_name and @p setter_name.
 *
 * @param StructType The structure type.
 * @param parent_field_name
 * The name (without quotes) of the existing integral field containing this bit field.
 * @p <parent_field_name>_proxy must have been defined within @p StructType.
 * @param field_name The bit field name without quotes.
 * @param getter_name The getter name without quotes.
 * @param setter_name The setter name without quotes.
 * @param bit_pos The offset in bits from the least significant bit of the parent field.
 */
#define DEFINE_BOOL_FIELD_WITH_PROXY(StructType, parent_field_name, field_name, getter_name, \
                                     setter_name, bit_pos)                                   \
public:                                                                                      \
    bool getter_name() const noexcept {                                                      \
        return field_name##_proxy.Get(*this);                                                \
    }                                                                                        \
                                                                                             \
    StructType& setter_name(const bool val) noexcept {                                       \
        field_name##_proxy.Set(*this, val);                                                  \
        return *this;                                                                        \
    }                                                                                        \
                                                                                             \
private:                                                                                     \
    inline static const auto field_name##_proxy {                                            \
        ::field_access_proxy::MakeBoolField(#field_name, parent_field_name##_proxy, bit_pos)};

/**
 * @brief Define a flexible array field with an associated proxy and accessors in a structure.
 *
 * @details
 * This macro declares the following elements within @p StructType:
 * - A private flexible array member @p first_<field_name> of type @p ElementType[1], used as the flexible array placeholder.
 * - A static private proxy field @p <field_name>_proxy of type @p FlexibleArrayField.
 * - Public member methods @p Get<field_name> and @p Set<field_name>.
 *
 * @param StructType The structure type.
 * @param ElementType The element type of the flexible array.
 * @param field_name The name of the flexible array field without quotes.
 * @param count_field_name
 * The field name without quotes that tracks the element count.
 * @p <count_field_name>_proxy must have been defined within @p StructType.
 *
 * @note The flexible array field is expected to be the last member of the structure.
 */
#define DEFINE_FLEXIBLE_ARRAY_FIELD_WITH_PROXY(StructType, ElementType, field_name,            \
                                               count_field_name)                               \
public:                                                                                        \
    auto Get##field_name() const noexcept {                                                    \
        return field_name##_proxy.GetAll(*this);                                               \
    }                                                                                          \
                                                                                               \
    StructType& Set##field_name(                                                               \
        const ::field_access_proxy::FlexibleArray<ElementType>& vals) noexcept {               \
        field_name##_proxy.SetAll(*this, vals, true);                                          \
        return *this;                                                                          \
    }                                                                                          \
                                                                                               \
private:                                                                                       \
    ElementType first_##field_name[1] {};                                                      \
    inline static const auto field_name##_proxy {::field_access_proxy::MakeFlexibleArrayField( \
        #field_name, &StructType::first_##field_name, count_field_name##_proxy)};
