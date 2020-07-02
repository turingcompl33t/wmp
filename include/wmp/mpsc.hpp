// mpsc.hpp

#include <windows.h>

#include <tuple>
#include <queue>
#include <memory>
#include <chrono>
#include <optional>

#include "detail/scoped_srw.hpp"
#include "detail/unique_srw.hpp"

namespace wmp::mpsc
{
    // ------------------------------------------------------------------------
    // detail::inner

    namespace detail
    {
        template <typename T>
        struct inner
        {
            SRWLOCK lock;

            CONDITION_VARIABLE nonfull;
            CONDITION_VARIABLE nonempty;

            std::queue<T> buffer;
            size_t const capacity;

            inner(size_t const capacity_) 
                : lock{}
                , nonfull{}
                , nonempty{}
                , buffer{}
                , capacity{capacity_} {}
        };
    }

    // ------------------------------------------------------------------------
    // sender

    enum class send_result
    {
        success,
        failure,
        timeout
    };

    template <typename T>
    class sender
    {
        std::shared_ptr<detail::inner<T>> m_inner;

    public:
        sender(std::shared_ptr<detail::inner<T>> inner)
            : m_inner{inner}
        {}

        // non-copyable, outside explicit clone()
        sender(sender const&) = delete;
        sender& operator=(sender const&) = delete;

        // default movable
        sender(sender&&)            = default;
        sender& operator=(sender&&) = default;

        auto clone() -> sender<T>
        {
            return sender{m_inner};
        }

        auto send(T value) -> send_result
        {
            using wmp::detail::unique_srw;
            using wmp::detail::srw_acquire;

            {
                auto lock = unique_srw{&m_inner->lock, srw_acquire::exclusive};
                
                // block until we acquire exclusive access on nonfull buffer
                while (m_inner->buffer.size() >= m_inner->capacity)
                {
                    ::SleepConditionVariableSRW(&m_inner->nonfull, &m_inner->lock, INFINITE, 0);
                }

                m_inner->buffer.push(value);
            }

            ::WakeConditionVariable(&m_inner->nonempty);
            return send_result::success;
        }

        // template <typename Duration>
        // auto send_timeout(T value, Duration timeout) -> send_result
        // {
        //     using wmp::detail::unique_srw;
        //     using wmp::detail::srw_acquire;

        //     auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);

        //     {
        //         auto lock = unique_srw{&m_inner->lock, srw_acquire::exclusive};
                
        //         // block until we acquire exclusive access on nonfull buffer
        //         while (m_inner->buffer.size() >= m_inner->capacity)
        //         {
        //             ::SleepConditionVariableSRW(
        //                 &m_inner->nonfull, 
        //                 &m_inner->lock, 
        //                 static_cast<unsigned long>(ms.count()),  // narrow
        //                 0);
        //         }

        //         m_inner->buffer.push(value);
        //     }

        //     ::WakeConditionVariable(&m_inner->nonempty);
        //     return send_result::success;
        // }

        auto try_send(T value) -> send_result
        {
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;

            auto sent = false;

            {
                auto guard = scoped_srw{&m_inner->lock, srw_acquire::exclusive};
                if (m_inner->buffer.size() < m_inner->capacity)
                {
                    m_inner->buffer.push(value);
                    sent = true;
                }
            }

            if (sent)
            {
                ::WakeConditionVariable(&m_inner->nonempty);
            }

            return sent ? send_result::success : send_result::failure;
        }
    };

    // ------------------------------------------------------------------------
    // receiver

    template <typename T>
    class receiver
    {
        std::shared_ptr<detail::inner<T>> m_inner;

    public:
        receiver(std::shared_ptr<detail::inner<T>> inner)
            : m_inner{inner}
        {}

        // non-copyable
        receiver(receiver const&)            = delete;
        receiver& operator=(receiver const&) = delete;

        // default movable
        receiver(receiver&&)            = default;
        receiver& operator=(receiver&&) = default;

        // recv() - indefinite blocking receive operation
        auto recv() -> std::optional<T>
        {
            using wmp::detail::unique_srw;
            using wmp::detail::srw_acquire;

            auto value = std::optional<T>{};  // std::nullopt

            {
                auto lock = unique_srw{&m_inner->lock, srw_acquire::exclusive};

                // block until we acquire exclusive access to nonempty buffer
                while (m_inner->buffer.size() == 0)
                {
                    ::SleepConditionVariableSRW(&m_inner->nonempty, &m_inner->lock, INFINITE, 0);
                }

                value.emplace(m_inner->buffer.front());
                m_inner->buffer.pop();
            }

            ::WakeConditionVariable(&m_inner->nonfull);
            return value;
        }

        // // recv_timeout() - blocking receive operation with timeout
        // template <typename Duration>
        // auto recv_timeout(Duration timeout) -> std::optional<T>
        // {

        // }

        // try_recv() - non-blocking receive operation
        auto try_recv() -> std::optional<T>
        {
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;
            
            auto value   = std::optional<T>{};  // std::nullopt
            auto removed = false;

            {
                auto guard = scoped_srw{&m_inner->lock, srw_acquire::exclusive};
                if (m_inner->buffer.size() > 0)
                {
                    value.emplace(m_inner->buffer.front());
                    m_inner->buffer.pop();

                    removed = true;
                }
            }

            if (removed)
            {
                ::WakeConditionVariable(&m_inner->nonfull);
            }

            return value;
        }
    };

    // ------------------------------------------------------------------------
    // create()

    template <typename T>
    auto create(size_t const capacity) -> std::pair<sender<T>, receiver<T>> 
    {
        auto shared_inner = std::make_shared<detail::inner<T>>(capacity);
        return std::pair{ sender{shared_inner}, receiver{shared_inner} };
    }
}