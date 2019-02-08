#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
#include <cassert>
#include <experimental/execution>
#include "par.hpp"
#include "for_each_n.hpp"

namespace execution = std::experimental::execution;

template<class OneWayExecutor>
class bulk_oneway_executor_adaptor
{
  public:
    bulk_oneway_executor_adaptor(const bulk_oneway_executor_adaptor&) = default;

    explicit bulk_oneway_executor_adaptor(const OneWayExecutor& executor) noexcept
      : executor_(executor)
    {}

    template<class Function, class Factory>
    void bulk_execute(Function f, size_t n, Factory shared_factory) const
    {
      executor_.execute([=]
      {
        auto shared = shared_factory();
        for(size_t i = 0; i != n; ++i)
        {
          f(i, shared);
        }
      });
    }

    bool operator==(const bulk_oneway_executor_adaptor& other) const noexcept
    {
      return executor_ == other.executor_;
    }

    bool operator!=(const bulk_oneway_executor_adaptor& other) const noexcept
    {
      return executor_ != other.executor_;
    }

    bulk_oneway_executor_adaptor require(execution::oneway_t) const
    {
      return *this;
    }

    template<class Property,
             class = std::enable_if_t<
               execution::can_require_v<OneWayExecutor, Property>
             >>
    auto require(const Property& prop) const
    {
      auto adapted_executor = execution::require(executor_, prop);
      return bulk_oneway_executor_adaptor<decltype(adapted_executor)>{adapted_executor};
    }

    // XXX what is the best way to provide both .query() and ::query()?
    //template<class Property,
    //         class = std::enable_if_t<
    //           execution::can_query_v<OneWayExecutor, Property>
    //         >>
    //constexpr auto query(const Property& prop) const
    //{
    //  return execution::query(executor_, prop);
    //}

    //template<class Property,
    //         class = std::enable_if_t<
    //           !execution::can_query_v<OneWayExecutor, Property>
    //         >,
    //         class = decltype(Property::template static_query_v<OneWayExecutor>)
    //        >
    //constexpr static auto query(const Property& prop)
    //{
    //  return Property::template static_query_v<OneWayExecutor>;
    //}

    // XXX workaround issues with forwarding queries above
    constexpr static execution::mapping_t::thread_t query(execution::mapping_t) noexcept
    {
      return execution::mapping.thread;
    }

    // XXX weaken this guarantee to satisfy .on() 
    //constexpr static execution::bulk_guarantee_t::sequenced_t query(execution::bulk_guarantee_t) noexcept
    //{
    //  return execution::bulk_guarantee.sequenced;
    //}
    constexpr static execution::bulk_guarantee_t::parallel_t query(execution::bulk_guarantee_t) noexcept
    {
      return execution::bulk_guarantee.parallel;
    }

  private:
    OneWayExecutor executor_;
};


class inline_executor
{
  public:
    friend bool operator==(const inline_executor&, const inline_executor&) noexcept
    {
      return true;
    }

    friend bool operator!=(const inline_executor&, const inline_executor&) noexcept
    {
      return false;
    }

    constexpr static execution::blocking_t::always_t query(execution::blocking_t)
    {
      return execution::blocking.always;
    }

    constexpr static execution::mapping_t::thread_t query(execution::mapping_t)
    {
      return execution::mapping.thread;
    }

    template<class Function>
    void execute(Function f) const noexcept
    {
      f();
    }
};


int main()
{
  {
    // test with adapted inline_executor, vector

    std::vector<int> vec(10);

    bulk_oneway_executor_adaptor<inline_executor> ex(inline_executor{});

    for_each_n(par.on(ex), vec.begin(), vec.size(), [](int& x)
    {
      x += 42;
    });

    assert(std::count(vec.begin(), vec.end(), 42) == static_cast<int>(vec.size()));
  }

  {
    // test with adapted static_thread_pool.executor(), vector

    std::vector<int> vec(10);

    std::experimental::static_thread_pool pool(4);
    bulk_oneway_executor_adaptor<std::experimental::static_thread_pool::executor_type> ex(pool.executor());

    for_each_n(par.on(ex), vec.begin(), vec.size(), [](int& x)
    {
      x += 42;
    });

    assert(std::count(vec.begin(), vec.end(), 42) == static_cast<int>(vec.size()));
  }

  std::cout << "OK" << std::endl;

  return 0;
}

