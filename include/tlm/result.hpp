#ifndef TLM_RESULT_HPP
#define TLM_RESULT_HPP

#include <type_traits>
#include <utility>
#include <variant>

namespace tlm {

template <typename T, typename E> class [[nodiscard]] Result {
    static_assert(!std::is_same_v<T, E>, "Result types cannot be the same!");
    static_assert(!std::is_convertible_v<T, E> && !std::is_convertible_v<E, T>,
                  "Result types must not be convertible into each other");

  public:
    constexpr Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : storage_{std::move(value)} {
    }
    constexpr Result(E error) noexcept(std::is_nothrow_move_constructible_v<E>)
        : storage_{std::move(error)} {
    }

    [[nodiscard]] constexpr const T *ok() const noexcept {
        return std::get_if<T>(&storage_);
    }
    [[nodiscard]] constexpr const E *err() const noexcept {
        return std::get_if<E>(&storage_);
    }

    template <typename OnOk, typename OnErr>
    constexpr decltype(auto) match(OnOk &&on_ok, OnErr &&on_err) const {
        return std::visit(
            [&](auto &&arg) -> decltype(auto) {
                using Alt = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<Alt, T>) {
                    return std::forward<OnOk>(on_ok)(arg);
                } else {
                    static_assert(std::is_same_v<Alt, E>);
                    return std::forward<OnErr>(on_err)(arg);
                }
            },
            storage_);
    }

  private:
    std::variant<T, E> storage_;
};

} // namespace tlm

#endif // TLM_RESULT_HPP
