// scoped_srw.hpp

#pragma once

#include <windows.h>

#include "srw_acquire.hpp"

namespace wmp::detail
{
    class scoped_srw
    {
        PSRWLOCK    m_lock;
        srw_acquire m_ownership;

    public:
        scoped_srw(PSRWLOCK lock, srw_acquire ownership)
            : m_lock{lock}
            , m_ownership{ownership}
        {
            if (srw_acquire::exclusive == m_ownership)
            {
                ::AcquireSRWLockExclusive(m_lock);
            }
            else
            {
                AcquireSRWLockShared(m_lock);
            }
        }

        ~scoped_srw()
        {
            release();
        }

        scoped_srw(scoped_srw const&)            = delete;
        scoped_srw& operator=(scoped_srw const&) = delete;

        scoped_srw(scoped_srw&&)            = delete;
        scoped_srw& operator=(scoped_srw&&) = delete;

    private:
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
        }
    };
}