// watch.hpp

#include <windows.h>

#include <tuple>

namespace wmp::watch
{   
    // ------------------------------------------------------------------------
    // inner

    namespace detail
    {   
        template <typename T>
        struct inner
        {   
            T       value;
            SRWLOCK value_lock;

            volatile ULONG_PTR version;

            bool closed;

            // notifed when all receiver handles drop
            SRWLOCK            closed_lock;
            CONDITION_VARIABLE closed_cv;
        };
    }

    // ------------------------------------------------------------------------
    // sender

    enum class send_result
    {
        success,
        failure
    };

    enum class wait_result
    {
        success,
        failure
    };

    template <typename T>
    class sender
    {
    public:
        sender()
        {

        }

        ~sender()
        {

        }

        sender(sender const&)            = delete;
        sender& operator=(sender const&) = delete;

        sender(sender&&)            = default;
        sender& operator=(sender&&) = default;

        // broadcast() - broadcast an update to all receiver handles
        auto broadcast(T value) -> send_result
        {

        }

        // wait_closed() - block sender instance until all receiver handles closed
        auto wait_closed() -> wait_result
        {

        }
    };

    // ------------------------------------------------------------------------
    // receiver

    template <typename T>
    class receiver
    {
    public:
        receiver()
        {

        }

        ~receiver()
        {

        }
        
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

        }

        // borrow() - return a reference to the most recently sent value
        //
        // The reference returned by borrow() includes an embedded lock guard
        // that holds a read lock on the internal value managed by the channel;
        // for this reason, outstanding borrows can block updates, so references
        // should only be held for short periods of time to minimize contention.
        auto borrow() -> void
        {

        }

        // recv() - attempts to clone the latest value sent via the channel
        auto recv() -> void
        {

        }
    };

    // ------------------------------------------------------------------------
    // create()

    template <typename T>
    auto create() -> std::pair<sender<T>, receiver<T>>
    {

    }
}