/* Def.hpp
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_COMMON_DEF_HPP__
#define SUNFISH_COMMON_DEF_HPP__

#if !defined(UNIX) && !defined(WIN32)
// windows
# ifdef _MSC_VER
#  ifndef WIN32
#   define WIN32 1
#  endif
// gcc family
# elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) ) && !defined(__MINGW32__)
#  ifndef UNIX
#   define UNIX 1
#  endif
// unknown
# else
#  define UNKNOWN_API 1
# endif
#endif

#if defined(WIN32)
# define NOMINMAX
# if _MSC_VER >= 1900
#  define CONSTEXPR constexpr
#  define CONSTEXPR_CONST constexpr
#  define NOEXCEPT noexcept
# else
#  define CONSTEXPR
#  define CONSTEXPR_CONST const
#  define NOEXCEPT
# endif
# define ALIGNAS(a) __declspec(align(a))
#else
# define CONSTEXPR constexpr
# define CONSTEXPR_CONST constexpr
# define NOEXCEPT noexcept
# define ALIGNAS(a) alignas(a)
#endif

#if !defined(BYTE_ORDER)
# define BIG_ENDIAN 1
# define LITTLE_ENDIAN 2
# if defined(WIN32)
#  define BYTE_ORDER LITTLE_ENDIAN
# else
#  if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#   define BYTE_ORDER BIG_ENDIAN
#   error "big endian is not supported"
#  elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#   define BYTE_ORDER LITTLE_ENDIAN
#  else
#   error "byte order is undefined"
#  endif
# endif
#endif

#endif // SUNFISH_COMMON_DEF_HPP__
