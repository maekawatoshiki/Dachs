#if !defined DACHS_HELPER_UTIL_HPP_INCLUDED
#define      DACHS_HELPER_UTIL_HPP_INCLUDED

#include <iterator>
#include <string>
#include <fstream>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <cstddef>

#include <boost/optional.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/irange.hpp>
#include <boost/iterator/zip_iterator.hpp>

namespace dachs {
namespace helper {

// Note: Never defined
extern void* enabler;

template <class String = std::string>
inline boost::optional<String> read_file(std::string const& file_name)
{
    typedef typename String::value_type CharT;

    std::basic_ifstream<CharT> input(file_name, std::ios::in);
    if (!input.is_open()) {
        return boost::none;
    }
    return String{std::istreambuf_iterator<CharT>{input},
                  std::istreambuf_iterator<CharT>{}};
}

namespace detail {

template<class T>
struct is_shared_ptr_impl : std::false_type
{};

template<class T>
struct is_shared_ptr_impl<std::shared_ptr<T>> : std::true_type
{};

template<bool...>
struct bools
{};

} // namespace detail

template<class T>
struct is_shared_ptr
    : detail::is_shared_ptr_impl<
          typename std::remove_cv<T>::type
      >
{};

template<bool B, class T = void>
using enable_if = typename std::enable_if<B, T>::type;

template<bool B, class T = void>
using disable_if = typename std::enable_if<!B, T>::type;

// When you don't want to copy result object, use boost::range_reference<T>::type
template<class Range, class Value>
inline boost::optional<typename Range::value_type>
find(Range const& r, Value const& v)
{
    auto const result = boost::find(r, v);
    if (result == boost::end(r)) {
        return boost::none;
    } else {
        return *result;
    }
}

// When you don't want to copy result object, use boost::range_reference<T>::type
template<class Range, class Predicate>
inline boost::optional<typename Range::value_type>
find_if(Range const& r, Predicate const& p)
{
    static_assert(std::is_same<decltype(p(std::declval<typename Range::value_type>())), bool>::value, "predicate doesn't return bool value");
    auto const result = boost::find_if(r, p);
    if (result == boost::end(r)) {
        return boost::none;
    } else {
        return *result;
    }
}

template<class T, class U>
inline bool any_of(std::initializer_list<T> const& list, U const& value)
{
    return std::any_of(std::begin(list), std::end(list), [&value](T const& v){ return v == value; });
}

template<class... Ranges>
inline auto zipped(Ranges &... ranges /*lvalue references*/)
{
    return boost::make_iterator_range(
            boost::make_zip_iterator(boost::make_tuple(boost::begin(ranges)...)),
            boost::make_zip_iterator(boost::make_tuple(boost::end(ranges)...))
        );
}

template<class... Args>
struct are_same
{
    static constexpr bool value
        = std::is_same<
            detail::bools<
                Args::value...
            >,
            detail::bools<
                (Args::value, true)...
            >
        >::value;
};

template<class I, typename std::enable_if<std::is_integral<I>::value>::type *& = helper::enabler>
inline auto indices(I const i) noexcept
{
    return boost::irange<I>(0, i);
}

template<class T>
inline auto indices(std::vector<T> const& v) noexcept
{
    return indices(v.size());
}

template<class I1, class I2>
inline auto indices(I1 const i1, I2 const i2) noexcept
{
    assert(i1 <= i2);
    return boost::irange<I1>(i1, static_cast<I1>(i2));
}

// Note:
// Check it is signed because i-1 may cause overflow
template<class I, typename std::enable_if<std::is_signed<I>::value>::type *& = helper::enabler>
inline auto rindices(I const i) noexcept
{
    assert(i >= 0);
    return boost::irange<I>(i-1, -1, -1);
}

template<class T>
inline auto rindices(std::vector<T> const& v) noexcept
{
    return rindices(static_cast<int>(v.size()));
}

template<class I1, class I2, typename std::enable_if<std::is_signed<I2>::value>::type *& = helper::enabler>
inline auto rindices(I1 const i1, I2 const i2) noexcept
{
    assert(i1 >= i2);
    return boost::irange<I2>(i2-1, static_cast<I2>(i1)-1, -1);
}

template<class AssociatedContainer, class T>
inline bool exists(AssociatedContainer const& c, T const& t) noexcept
{
    return c.find(t) != std::end(c);
}

template<class T>
inline std::string hex_string_of_ptr(T const* const t)
{
    std::stringstream s;
    s << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(t);
    return s.str();
}

template<class T, class Predicate>
inline auto remove_erase_if(std::vector<T> &v, Predicate const& predicate)
{
    return v.erase(
                std::remove_if(
                    std::begin(v),
                    std::end(v),
                    predicate
                ),
                std::end(v)
            );
}

template<class T, class Predicate>
inline boost::optional<std::size_t> index_of(std::vector<T> const& v, Predicate const& predicate)
{
    for (auto i = 0u, size = v.size(); i < size; ++i) {
        if (predicate(v[i])) {
            return i;
        }
    }
    return boost::none;
}

template<class T>
inline boost::optional<std::size_t> index_of(std::vector<T> const& v, typename std::vector<T>::value_type const& value)
{
    for (auto i = 0u, size = v.size(); i < size; ++i) {
        if (v[i] == value) {
            return i;
        }
    }
    return boost::none;
}

template<class Int>
inline std::string to_ordinal(Int const i)
{
    auto const d = i % 10;

    switch(d) {
    case 1:
        return std::to_string(i) + "st";
    case 2:
        return std::to_string(i) + "nd";
    case 3:
        return std::to_string(i) + "rd";
    default:
        return std::to_string(i) + "th";
    }
}

template<class T, class Pred>
inline T tap(T && v, Pred &&pred)
{
    std::forward<Pred>(pred)(std::forward<T>(v));
    return v;
}

} // namespace helper
}  // namespace dachs

#endif    // DACHS_HELPER_UTIL_HPP_INCLUDED
