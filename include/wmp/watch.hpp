// watch.hpp

#pragma once

#include <windows.h>

#include <memory>
#include <atomic>
#include <utility>
#include <optional>

#include <wmp/detail/scoped_srw.hpp>
#include <wmp/detail/unique_srw.hpp>

namespace wmp::watch
{   
    // ------------------------------------------------------------------------
    // version constants

    namespace detail
    {
        constexpr static uint64_t const CLOSED    = 1;  // low bit reserved for "closed" flag
        constexpr static uint64_t const VERSION_0 = 0;
        constexpr static uint64_t const VERSION_1 = 2;
    }

    // ------------------------------------------------------------------------
    // inner

    namespace detail
    {   
        template <typename T>
        struct inner
        {   
            T object;

            // read lock acquired by receiver(s) on receiver::borrow()
            // write lock acquired by sender on sender::broadcast()
            SRWLOCK            object_lock;
            // notified on update to stored object
            CONDITION_VARIABLE object_cv;

            // the latest published version
            std::atomic_uint64_t version;

            inner(T init)
                : object{init}
                , object_lock{}
                , object_cv{}
                // VERSION_0 reserved for receivers that do not "know" initial state
                , version{VERSION_1}  
            {
                ::InitializeSRWLock(&object_lock);
                ::InitializeConditionVariable(&object_cv);
            }

            ~inner() = default;
        };
    }

    // ------------------------------------------------------------------------
    // sender

    // TODO: transition to expected<>
    enum class send_result
    {
        success,
        failure
    };

    template <typename T>
    class sender
    {
        std::weak_ptr<detail::inner<T>> m_shared;

    public:
        sender(std::weak_ptr<detail::inner<T>> shared) 
            : m_shared{shared} {}

        ~sender()
        {
            // notify any receiver handles that are waiting on updates
            // that no future updates will be broadcast
            if (auto shared = m_shared.lock())
            {
                std::atomic_fetch_or(&shared->version, detail::CLOSED);
                ::WakeAllConditionVariable(&shared->object_cv);
            }
        }

        // non-copyable;
        // only a single sender is permitted for watch channel
        sender(sender const&)            = delete;
        sender& operator=(sender const&) = delete;

        // default-movable (rely on move semantics of std::weak_ptr)
        sender(sender&&)            = default;
        sender& operator=(sender&&) = default;

        // broadcast() - broadcast an update to all receiver handles
        //
        // TODO: migrate to expected<>
        auto broadcast(T object) -> send_result
        {
            using wmp::detail::scoped_srw;
            using wmp::detail::srw_acquire;

            auto shared = m_shared.lock();
            if (!shared)
            {
                // the channel is closed; all receiver handles have dropped
                return send_result::failure;
            }

            // at least one receiver handle is still valid;
            // the local shared_ptr that results from lock()
            // extends the lifetime of the shared state to 
            // (at least) the end of this function body

            {
                // acquire right access to the object;
                // all outstanding borrow()s block write at this point 
                auto guard = scoped_srw{&m_shared->object_lock, srw_acquire::exclusive};
                m_shared->object = object;
            }

            // increment the version number;
            // increment by 2 ensures that CLOSED bit never set
            std::atomic_fetch_add(&m_shared->version, 2);

            // wake all receivers waiting on an update
            ::WakeAllConditionVariable(&m_shared->object_cv);

            return send_result::success;
        }

        // closed() - determine if all receiver handles have been dropped
        //
        // NOTE: there is a fundamental difference between the functionality
        // implemented here and that provided by tokio::sync::watcher::closed(),
        // namely that the implementation in tokio is an asynchronous function
        // that only returns Ready() when the channel is closed and does not 
        // force the user to poll the state of the channel as in this implementation
        auto closed() const noexcept -> bool
        {
            // we rely on the semantics of std::shared_ptr 
            // to manage the lifetime of the shared state 
            return static_cast<bool>(m_shared.lock());
        }
    };

    // ------------------------------------------------------------------------
    // borrow

    template <typename T>
    class borrow
    {
        using unique_srw = wmp::detail::unique_srw;

        T const&   m_object;
        unique_srw m_read_lock;

    public:
        borrow(
            T const&     object, 
            unique_srw&& read_lock)
            : m_object{object} 
            , m_read_lock{std::move(read_lock)}
        {}

        ~borrow() = default;

        // non-copyable
        borrow(borrow const&)            = delete;
        borrow& operator=(borrow const&) = delete;

        // default-movable
        borrow(borrow&&)            = default;
        borrow& operator=(borrow&&) = default;

        // borrowed read reference accessible via operator*
        T const& operator*() const noexcept
        {
            return m_object;
        }
    };

    // ------------------------------------------------------------------------
    // receiver

    // TODO: migrate to expected<>
    enum class recv_result
    {
        success,
        failure
    };

    template <typename T>
    class receiver
    {
        std::atomic_uint64_t              m_version;
        std::shared_ptr<detail::inner<T>> m_shared;

    public:
        receiver(
            uint64_t const                    version, 
            std::shared_ptr<detail::inner<T>> shared)
            : m_version{version} 
            , m_shared{shared}
        {}

        ~receiver() = default;
        
        // non-copyable;
        // use explicit clone() to create new receiver handle 
        receiver(receiver const&)            = delete;
        receiver& operator=(receiver const&) = delete;

        // default-movable
        receiver(receiver&&)            = default;
        receiver& operator=(receiver&&) = default;

        // clone() - create a new receiver handle
        auto clone() -> receiver<T>
        {
            // cloned receiver inherits version
            auto const version 
                = std::atomic_load_explicit(&m_version, std::memory_order_relaxed);
            return receiver{version, m_shared};
        }

        // borrow() - return a reference to the most recently sent value
        //
        // The reference returned by borrow() includes an embedded lock guard
        // that holds a read lock on the internal value managed by the channel;
        // for this reason, outstanding borrows can block updates, so references
        // should only be held for short periods of time to minimize contention.
        auto borrow() -> borrow<T>
        {
            using wmp::detail::unique_srw;
            using wmp::detail::srw_acquire;

            auto lock = unique_srw{&m_shared->object_lock, srw_acquire::shared};
            return borrow{m_shared->object, std::move(lock)};
        }

        // recv() - attempts to clone the latest value sent via the channel
        //
        // TODO: need to constrain this to only cases in which T is copyable
        //       constrain via std::enable_if?
        // TODO: migrate to expected<>
        auto recv() -> std::optional<T>
        {
            using wmp::detail::unique_srw;
            using wmp::detail::srw_acquire;

            // load the version present in shared state
            // represents the latest version published by sender
            auto const state   = std::atomic_load(&m_shared->version);
            auto const version = (state & !detail::CLOSED);

            // update the local version to the latest version
            if (version != std::atomic_exchange_explicit(
                &m_version, 
                version, 
                std::memory_order_relaxed))
            {
                // local version did not match the latest version; update available
                // this construction allows the receiver to receive the update even
                // in the event that the channel is closed at this point;
                // the next time recv() is called the receiver will "notice" the closure
            }

            if (detail::CLOSED == (state & detail::CLOSED))
            {
                // the channel was closed by sender (sender handle dropped)
                return std::nullopt;
            }

            // local version is up to date and channel is not closed; wait for broadcast
            auto published = std::atomic_load(&m_shared->version);
            auto lock      = unique_srw{&m_shared->object_lock, srw_acquire::shared};
            while (std::atomic_load_explicit(&m_version, std::memory_order_relaxed) == published)
            {
                ::SleepConditionVariableSRW(&m_shared->object_cv, &m_shared->object_lock, INFINITE, 0);
                published = std::atomic_load(&m_shared->version);
            }

            // update the local version
            std::atomic_store_explicit(&m_version, published, std::memory_order_relaxed);

            // return the published value; access is safe because read lock is held
            return std::make_optional<T>(m_shared->object);
        }
    };

    // ------------------------------------------------------------------------
    // create()

    template <typename T>
    auto create(T init) -> std::pair<sender<T>, receiver<T>>
    {
        // TODO: move rather than copy? explicit ownership
        auto shared = std::make_shared<detail::inner<T>>(init);
        auto weak   = std::weak_ptr{shared};
        return std::pair{
            sender<T>{weak}, 
            receiver<T>{detail::VERSION_0, shared}};
    }
}