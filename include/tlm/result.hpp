#ifndef TLM_RESULT_HPP
#define TLM_RESULT_HPP

#include <utility>
#include <variant>

namespace tlm {

template <typename T, typename E> class [[nodiscard]] Result {
    static_assert(!std::is_same_v<T, E>, "Result types cannot be the same!");

  public:
    constexpr Result(T value) noexcept : storage_{std::move(value)} {
    }
    constexpr Result(E error) noexcept : storage_{std::move(error)} {
    }

    [[nodiscard]] constexpr const T *ok() const noexcept {
        return nullptr;
    }
    [[nodiscard]] constexpr const E *err() const noexcept {
        return nullptr;
    }

    template <typename OnOk, typename OnErr>
    constexpr decltype(auto) match(OnOk &&on_ok, OnErr && /*on_err*/) const {
        return std::forward<OnOk>(on_ok)(T{});
    }

  private:
    std::variant<T, E> storage_;
};

} // namespace tlm

#endif // TLM_RESULT_HPP
