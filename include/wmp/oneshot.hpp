// oneshot.hpp

#pragma once

#include <tuple>
#include <memory>
#include <optional>

#include "detail/scoped_srw.hpp"
#include "detail/unique_srw.hpp"

namespace wmp::oneshot
{
    // ------------------------------------------------------------------------
    // detail::state

    namespace detail
    {
        enum class state
        {
            init,
            sent,
            wait_send,
            wait_recv,
            closed,
            closed_recv
        };

        auto swap_state(state& current, state updated) -> state
        {
            auto const tmp = current;
            current = updated;
            return tmp;
        }

        auto is_closed(state const s) -> bool
        {
            return state::closed == s || state::closed_recv == s;
        }
    }

    // ------------------------------------------------------------------------
    // detail::inner

    namespace detail
    {
        template<typename T>
        struct inner
        {
            // mutual exclusion
            SRWLOCK lock;

            // notified upon receiver drop
            CONDITION_VARIABLE tx_cv;
            // notified upon sender send()
            CONDITION_VARIABLE rx_cv;

            state state;

            std::optional<T> value;

            inner()
                : lock{}
                , tx_cv{}
                , rx_cv{}
                , state{state::init}
                , value{std::nullopt}
            {
                ::InitializeSRWLock(&lock);
                ::InitializeConditionVariable(&tx_cv);
                ::InitializeConditionVariable(&rx_cv);
            }

            ~inner() = default;

            inner(inner const&)            = delete;
            inner& operator=(inner const&) = delete;

            inner(inner&&)            = delete;
            inner& operator=(inner&&) = delete;
        };
    }

    // ------------------------------------------------------------------------
    // sender

    enum class send_result
    {
        success,
        failure
    };

    template<typename T>
    class sender
    {
        std::shared_ptr<detail::inner<T>> m_inner;

    public:
        sender(std::shared_ptr<detail::inner<T>> inner)
            : m_inner{inner}
        {}

        ~sender()
        {
            // don't attempt to close the channel when in moved-from state
            if (m_inner)
            {
                close();
            }
        }

        // non-copyable
        sender(sender const&)            = delete;
        sender& operator=(sender const&) = delete;

        // default-movable
        sender(sender&&)            = default;
        sender& operator=(sender&&) = default;

        // send_async() - send value to receiver without waiting for completion
        //
        // send_async() simply emplaces the given value in the channel
        // and returns immediately, without waiting for the receiver to 
        // take any action.
        auto send_async(T value) -> send_result
        {
            using detail::state;
            using detail::swap_state;
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;

            auto prev = state::init;

            {
                auto guard = scoped_srw{&m_inner->lock, srw_acquire::exclusive};
                if (is_closed(m_inner->state))
                {
                    return send_result::failure;
                }

                m_inner->value = value;
                prev = swap_state(m_inner->state, state::sent);
            }

            if (state::wait_recv == prev)
            {
                ::WakeConditionVariable(&m_inner->rx_cv);
            }

            return send_result::success;
        }

        // send_sync() - send value to receiver, waiting for successful recv() or close
        // 
        // The receive operation is considered complete under a number of conditions, including:
        //  - when the receiver receives the sent value
        //  - when the receiver handle goes out of scope, without receiving the value
        //  - when close() is explicitly called on receiver handle, without receiving the value
        auto send_sync(T value) -> send_result
        {
            using detail::state;
            using detail::swap_state;
            using wmp::detail::unique_srw;
            using wmp::detail::srw_acquire;

            auto prev = state::init;

            {
                auto lock = unique_srw{&m_inner->lock, srw_acquire::exclusive};
                if (is_closed(m_inner->state))
                {
                    return send_result::failure;
                }
                else if (state::wait_send == m_inner->state)
                {
                    // receiver already waiting on send()
                    m_inner->value = value;
                    prev = swap_state(m_inner->state, state::sent);
                }
                else
                {
                    // receiver not yet waiting on send()
                    m_inner->value = value;
                    swap_state(m_inner->state, state::wait_recv);
                    while (state::wait_recv == m_inner->state)
                    {
                        ::SleepConditionVariableSRW(&m_inner->tx_cv, &m_inner->lock, INFINITE, 0);
                    }

                    prev = swap_state(m_inner->state, state::closed);
                }
            }

            // when waiting for send() to complete synchonously, the receiver may close the
            // channel explicitly or drop the receiver handle, so we distinguish between a 
            // successful synchronous send() and a failed one
            return state::closed_recv == prev 
                ? send_result::success 
                : send_result::failure;
        }

        // close() - explicitly close the channel
        //
        // Both the sender and the receiver can explicitly close the channel
        // before or after a value is sent across it.
        auto close() -> void
        {
            using detail::state;
            using detail::swap_state;
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;

            auto prev = state::init;

            {
                auto guard = scoped_srw{&m_inner->lock, srw_acquire::exclusive};
                prev = swap_state(m_inner->state, state::closed);
            }

            if (state::wait_send == prev)
            {
                // notify receiver that channel has been closed
                ::WakeConditionVariable(&m_inner->rx_cv);
            }
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

        ~receiver()
        {
            // don't attempt to close the channel when in moved-from state
            if (m_inner)
            {
                close();
            }
        }

        // non-copyable
        receiver(receiver const&)           = delete;
        receiver operator=(receiver const&) = delete;

        // default-movable
        receiver(receiver&&)            = default;
        receiver& operator=(receiver&&) = default;

        // recv() - blocking receive operation
        auto recv() -> std::optional<T>
        {
            using detail::state;
            using detail::swap_state;
            using wmp::detail::unique_srw;
            using wmp::detail::srw_acquire;

            auto value = std::optional<T>{}; // std::nullopt
            auto prev  = state::init;

            {
                auto lock = unique_srw{&m_inner->lock, srw_acquire::exclusive};
                if (is_closed(m_inner->state))
                {
                    return value;
                }
                else if (state::sent == m_inner->state ||
                         state::wait_recv == m_inner->state)
                {
                    // value is available, sender possibly waiting
                    value.swap(m_inner->value);
                    prev = swap_state(m_inner->state, state::closed_recv);
                }
                else
                {
                    // value is not yet ready; wait for sender to make progress
                    swap_state(m_inner->state, state::wait_send);
                    while (state::wait_recv == m_inner->state)
                    {
                        ::SleepConditionVariableSRW(&m_inner->rx_cv, &m_inner->lock, INFINITE, 0);
                    }

                    value.swap(m_inner->value);
                    prev = swap_state(m_inner->state, state::closed_recv);
                }
            }

            if (state::wait_recv == prev)
            {
                // sender waiting on recv(), notify
                ::WakeConditionVariable(&m_inner->tx_cv);
            }

            return value;
        }

        // try_recv() - non-blocking receive operation
        auto try_recv() -> std::optional<T>
        {
            using detail::state;
            using detail::swap_state;
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;

            auto value = std::optional<T>{}; // std::nullopt
            auto prev  = state::init;

            {
                auto guard = scoped_srw{&m_inner->lock, srw_acquire::exclusive};

                if (is_closed(m_inner->state))
                {
                    return value;
                }
                
                if (state::sent == m_inner->state || 
                    state::wait_recv == m_inner->state)
                {
                    // value is available, sender possibly waiting on recv completion
                    value.swap(m_inner->value);
                    prev = swap_state(m_inner->state, state::closed_recv);
                }
            }

            if (state::wait_recv == prev)
            {
                // sender waiting on recv(), notify
                ::WakeConditionVariable(&m_inner->tx_cv);
            }

            return value;
        }

        // close() - explicitly close the channel
        //
        // Once close() completes, it is no longer possible for a sender to send() a value through
        // the channel. However, it is possible that even though close() is called, a value is
        // sent through the channel in the event that the send() operation initiates before the
        // call to close(). For this reason, graceful shutdown should include a call to try_recv()
        // after a call to close() to check for the presence of a sent value.
        auto close() -> void
        {
            using detail::state;
            using detail::swap_state;
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;

            auto prev = state::init;

            {
                auto guard = scoped_srw{&m_inner->lock, srw_acquire::exclusive};
                prev = swap_state(m_inner->state, state::closed);
            }

            if (state::wait_recv == prev)
            {
                // notify sender that channel has been closed
                ::WakeConditionVariable(&m_inner->tx_cv);
            }
        }
    };

    // ------------------------------------------------------------------------
    // create()

    // create() - construct a new oneshot channel
    //
    // create() simply constructs the shared inner state of which
    // the channel is composed and returns a std::pair of the sender
    // and receiver objects that wrap the state and enable interaction
    // with the channel itself.
    template <typename T>
    auto create() -> std::pair<sender<T>, receiver<T>> 
    {
        auto shared_inner = std::make_shared<detail::inner<T>>();
        return std::pair{ sender<T>{shared_inner}, receiver<T>{shared_inner} };
    }
}