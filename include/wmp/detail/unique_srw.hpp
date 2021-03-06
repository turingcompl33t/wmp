// unique_srw.hpp

#pragma once

#include <windows.h>

#include "srw_acquire.hpp"

namespace wmp::detail
{
    class lock_error {};

    class unique_srw
    {
        enum class state
        {
            locked,
            unlocked
        };

        PSRWLOCK    m_lock;
        state       m_state;
        srw_acquire m_ownership;

    public:
        unique_srw(PSRWLOCK lock, srw_acquire ownership)
            : m_lock{lock}
            , m_state{state::unlocked}
            , m_ownership{ownership}
        {
            acquire();
        }

        ~unique_srw()
        {
            if (owns_lock())
            {
                release();
            }
        }  

        // non-copyable
        unique_srw(unique_srw const&)            = delete;
        unique_srw& operator=(unique_srw const&) = delete;

        // move-constructible
        // QUESTION: can this be defaulted?
        unique_srw(unique_srw&& other) noexcept
            : m_lock{other.m_lock}
            , m_state{other.m_state}
            , m_ownership{other.m_ownership}
        {}

        // move-assignable
        // QUESTION: can this be defaulted?
        unique_srw& operator=(unique_srw&& rhs) noexcept
        {
            if (this != &rhs)
            {
                if (owns_lock())
                {
                    release();
                }

                m_lock      = rhs.m_lock;
                m_state     = rhs.m_state;
                m_ownership = rhs.m_ownership;

                rhs.m_lock  = nullptr;
                rhs.m_state = state::unlocked;
            }

            return *this;
        }

        auto lock() -> void
        {
            if (owns_lock())
            {
                throw lock_error{};
            }

            acquire();
        }

        auto unlock() -> void
        {
            if (!owns_lock())
            {
                throw lock_error{};
            }

            release();
        }

        auto owns_lock() -> bool
        {
            return state::locked == m_state;
        }

    private:
        auto acquire() -> void
        {
            if (srw_acquire::exclusive == m_ownership)
            {
                ::AcquireSRWLockExclusive(m_lock);
            }
            else
            {
                ::AcquireSRWLockShared(m_lock);
            }

            m_state = state::locked;
        }

        auto release() -> void
        {
            if (srw_acquire::exclusive == m_ownership)
            {
                ::ReleaseSRWLockExclusive(m_lock);
            }
            else
            {
                ::ReleaseSRWLockShared(m_lock);
            }

            m_state = state::unlocked;
        }
    };
}