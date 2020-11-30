#include <fmt/format.h>
#include "dynamic_stack.hpp"
#include "doctest.h"

#include <atomic>

struct S {
  static inline std::atomic<int> n_instances;
  S() { ++n_instances; }
  ~S() { --n_instances; }
};

DOCTEST_TEST_CASE("raii") {
  unsigned char buf[4096];
  veg::dynamic_stack_view stack(buf, 4096);

  {
    auto s1 = stack.make_new<S>(3);
    DOCTEST_CHECK(s1.data() != nullptr);
    DOCTEST_CHECK(s1.size() == 3);
    DOCTEST_CHECK(stack.remaining_bytes() == 4093);
    DOCTEST_CHECK(S::n_instances == 3);

    {
      auto s2 = stack.make_new<S>(4);
      DOCTEST_CHECK(s2.data() != nullptr);
      DOCTEST_CHECK(s2.size() == 4);
      DOCTEST_CHECK(stack.remaining_bytes() == 4089);
      DOCTEST_CHECK(S::n_instances == 7);

      {
        auto i3 = stack.make_new<int>(30000);
        DOCTEST_CHECK(i3.data() == nullptr);
        DOCTEST_CHECK(i3.size() == 0);
        DOCTEST_CHECK(stack.remaining_bytes() == 4089);
        {
          auto i4 = stack.make_new<int>(300);
          DOCTEST_CHECK(i4.data() != nullptr);
          DOCTEST_CHECK(i4.size() == 300);
          DOCTEST_CHECK(stack.remaining_bytes() < 4089 - 300 * sizeof(int));
        }
      }
    }
    DOCTEST_CHECK(stack.remaining_bytes() == 4093);
    DOCTEST_CHECK(S::n_instances == 3);
  }
  DOCTEST_CHECK(stack.remaining_bytes() == 4096);
  DOCTEST_CHECK(S::n_instances == 0);

  auto s1 = stack.make_new<S const>(3);
  DOCTEST_CHECK(stack.remaining_bytes() == 4093);
  DOCTEST_CHECK(S::n_instances == 3);
}

DOCTEST_TEST_CASE("return") {
  unsigned char buf[4096];
  veg::dynamic_stack_view stack(buf, 4096);

  auto s = [&] {
    auto s1 = stack.make_new<S>(3);
    auto s2 = stack.make_new<S>(4);
    auto s3 = stack.make_new<S>(5);
    DOCTEST_CHECK(stack.remaining_bytes() == 4084);
    DOCTEST_CHECK(S::n_instances == 12);
    return s1;
  }();

  DOCTEST_CHECK(stack.remaining_bytes() == 4093);
  DOCTEST_CHECK(S::n_instances == 3);

  DOCTEST_CHECK(s.data() != nullptr);
  DOCTEST_CHECK(s.size() == 3);
}

DOCTEST_TEST_CASE("manual management") {
  unsigned char buf[4096];
  veg::dynamic_stack_view stack(buf, 4096);

  auto s = stack.make_alloc(veg::tag<S>, 3);
  DOCTEST_CHECK(s.data() != nullptr);
  DOCTEST_CHECK(s.size() == 3);
  DOCTEST_CHECK(S::n_instances == 0);

  {
    new (s.data() + 0) S{};
    DOCTEST_CHECK(S::n_instances == 1);
    new (s.data() + 1) S{};
    DOCTEST_CHECK(S::n_instances == 2);
    new (s.data() + 2) S{};
    DOCTEST_CHECK(S::n_instances == 3);

    (s.data() + 2)->~S();
    DOCTEST_CHECK(S::n_instances == 2);
    (s.data() + 1)->~S();
    DOCTEST_CHECK(S::n_instances == 1);
    (s.data() + 0)->~S();
    DOCTEST_CHECK(S::n_instances == 0);
  }
  DOCTEST_CHECK(S::n_instances == 0);
}
