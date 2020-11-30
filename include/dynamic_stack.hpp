#ifndef DYNAMIC_STACK_DYNAMIC_STACK_HPP_UBOMZFTOS
#define DYNAMIC_STACK_DYNAMIC_STACK_HPP_UBOMZFTOS

#include <new>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <type_traits>

namespace veg {

namespace _dynstack {
template <bool B, typename T>
struct enable_if {
  using type = T;
};
template <typename T>
struct enable_if<false, T> {};

template <bool B, typename T>
using enable_if_t = typename enable_if<B, T>::type;

using std::size_t;

inline auto
align_next(size_t alignment, size_t size, void*& ptr, size_t& space) noexcept
    -> void* {
  static_assert(
      sizeof(std::uintmax_t) >= sizeof(void*),
      "uintmax can't hold a pointer value");

  using byte_ptr = unsigned char*;

  // assert alignment is power of two
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    std::terminate();
  }

  if (space < size) {
    return nullptr;
  }

  std::uintmax_t lo_mask = alignment - 1;
  std::uintmax_t hi_mask = ~lo_mask;

  auto const intptr = reinterpret_cast<std::uintmax_t>(ptr);
  auto* const byteptr = static_cast<byte_ptr>(ptr);

  auto offset = ((intptr + alignment - 1) & hi_mask) - intptr;

  if (space - size < offset) {
    return nullptr;
  }

  void* const rv = byteptr + offset;

  ptr = byteptr + (offset + size);
  space = space - (offset + size);

  return rv;
}

struct default_init_fn {
  template <typename T>
  auto make(void* ptr, size_t len) -> T* {
    return new (ptr) T[len];
  }
};

struct zero_init_fn {
  template <typename T>
  auto make(void* ptr, size_t len) -> T* {
    return new (ptr) T[len]{};
  }
};

#if defined(__has_builtin)
#if __has_builtin(__builtin_launder)
#define DYNAMIC_STACK_LAUNDER(p) __builtin_launder(p)
#endif
#elif __GNUC__ >= 7
#define DYNAMIC_STACK_LAUNDER(p) __builtin_launder(p)
#endif

#ifndef DYNAMIC_STACK_LAUNDER
#if __cplusplus >= 201703L
#define DYNAMIC_STACK_LAUNDER(p) std::launder(p)
#else
#define DYNAMIC_STACK_LAUNDER(p) (p)
#endif
#endif

struct no_init_fn {
  template <typename T>
  auto make(void* ptr, size_t len) -> T* {
    return DYNAMIC_STACK_LAUNDER(static_cast<T*>(
        static_cast<void*>(new (ptr) unsigned char[len * sizeof(T)])));
  }
};

#undef DYNAMIC_STACK_LAUNDER

template <typename T>
struct tag_t;
} // namespace _dynstack

// workaround for lack of variable templates
template <typename T>
static auto tag() -> _dynstack::tag_t<T>* {
  return nullptr;
}

struct dynamic_stack_view {
private:
  template <typename T>
  struct dynamic_buffer;
  template <typename T>
  struct manually_managed_dynamic_buffer;

public:
  dynamic_stack_view(void* data, std::size_t n_bytes) noexcept
      : m_data(data), m_rem_bytes(n_bytes) {}

  auto remaining_bytes() const noexcept -> std::size_t { return m_rem_bytes; }

  template <typename T>
  auto make_new(std::size_t len, std::size_t align = alignof(T)) //

      noexcept(std::is_nothrow_default_constructible<T>::value)

          -> _dynstack::enable_if_t<
              (std::is_default_constructible<T>::value &&
               std::is_destructible<T>::value),
              dynamic_buffer<T>> {
    return {*this, len, align, _dynstack::zero_init_fn{}};
  }
  template <typename T>
  auto make_new(
      _dynstack::tag_t<T>* (*/*unused*/)(),
      std::size_t len,
      std::size_t align = alignof(T)) //

      noexcept(std::is_nothrow_default_constructible<T>::value)

          -> _dynstack::enable_if_t<
              (std::is_default_constructible<T>::value &&
               std::is_destructible<T>::value),
              dynamic_buffer<T>> {
    return {*this, len, align, _dynstack::zero_init_fn{}};
  }

  template <typename T>
  auto make_new_for_overwrite(
      std::size_t len,
      std::size_t align = alignof(T)) //

      noexcept(std::is_nothrow_default_constructible<T>::value)

          -> _dynstack::enable_if_t<
              (std::is_default_constructible<T>::value &&
               std::is_destructible<T>::value),
              dynamic_buffer<T>> {
    return {*this, len, align, _dynstack::default_init_fn{}};
  }
  template <typename T>
  auto make_new_for_overwrite(
      _dynstack::tag_t<T>* (*/*unused*/)(),
      std::size_t len,
      std::size_t align = alignof(T)) //

      noexcept(std::is_nothrow_default_constructible<T>::value)

          -> _dynstack::enable_if_t<
              (std::is_default_constructible<T>::value &&
               std::is_destructible<T>::value),
              dynamic_buffer<T>> {
    return {*this, len, align, _dynstack::default_init_fn{}};
  }

  template <typename T>
  auto make_alloc(std::size_t len, std::size_t align = alignof(T)) noexcept
      -> _dynstack::enable_if_t<
          (std::is_destructible<T>::value),
          manually_managed_dynamic_buffer<T>> {
    return {*this, len, align, _dynstack::no_init_fn{}};
  }
  template <typename T>
  auto make_alloc(
      _dynstack::tag_t<T>* (*/*unused*/)(),
      std::size_t len,
      std::size_t align = alignof(T)) noexcept
      -> _dynstack::enable_if_t<
          (std::is_destructible<T>::value),
          manually_managed_dynamic_buffer<T>> {
    return {*this, len, align, _dynstack::no_init_fn{}};
  }

private:
  template <typename T>
  struct manually_managed_dynamic_buffer {
    template <typename Fn>
    manually_managed_dynamic_buffer(
        dynamic_stack_view& parent,
        std::size_t len,
        std::size_t align,
        Fn fn) noexcept(noexcept(T()))
        : m_parent(parent), m_old_pos(parent.m_data) {

      void* data = _dynstack::align_next(
          align, len * sizeof(T), m_parent.m_data, m_parent.m_rem_bytes);

      if (data != nullptr) {
        m_len = len;
        m_data = fn.template make<T>(data, len);
      }
    }

    manually_managed_dynamic_buffer(manually_managed_dynamic_buffer const&) =
        delete;
    manually_managed_dynamic_buffer(
        manually_managed_dynamic_buffer&& other) noexcept
        : m_parent(other.m_parent),
          m_old_pos(other.m_old_pos),
          m_data(other.m_data),
          m_len(other.m_len) {
      other.m_len = 0;
      other.m_data = nullptr;
    };

    auto operator=(manually_managed_dynamic_buffer)
        -> manually_managed_dynamic_buffer& = delete;

    auto data() const noexcept -> T* { return m_data; }
    auto size() const noexcept -> std::size_t { return m_len; }

  protected:
    dynamic_stack_view& m_parent;
    void* m_old_pos;
    T* m_data = nullptr;
    std::size_t m_len = 0;
  };

  template <typename T>
  struct dynamic_buffer : private manually_managed_dynamic_buffer<T> {
    using manually_managed_dynamic_buffer<T>::manually_managed_dynamic_buffer;
    using manually_managed_dynamic_buffer<T>::data;
    using manually_managed_dynamic_buffer<T>::size;
    dynamic_buffer(dynamic_buffer const&) = delete;
    dynamic_buffer(dynamic_buffer&&) noexcept = default;
    auto operator=(dynamic_buffer) -> dynamic_buffer& = delete;

    ~dynamic_buffer() {
      if (this->m_len != 0) {
        if (this->m_parent.m_data != (this->m_data + this->m_len)) {
          // in case weird resources are reodered by moving ownership
          std::terminate();
        }
        if (this->m_parent.m_data != (this->m_data + this->m_len)) {
          if (static_cast<unsigned char*>(this->m_parent.m_data) <
              static_cast<unsigned char*>(this->m_old_pos)) {
            std::terminate(); // safety check
          }
        }
        for (std::size_t i = 0; i < this->m_len; ++i) {
          (this->m_data + i)->~T();
        }
        this->m_parent.m_rem_bytes += static_cast<std::size_t>(
            static_cast<unsigned char*>(this->m_parent.m_data) -
            static_cast<unsigned char*>(this->m_old_pos));
        this->m_parent.m_data = this->m_old_pos;
      }
    }
  };

  void* m_data;
  std::size_t m_rem_bytes;
};

} // namespace veg

#endif /* end of include guard DYNAMIC_STACK_DYNAMIC_STACK_HPP_UBOMZFTOS */
