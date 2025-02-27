// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// thread functions

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <process.h>
#include <xthreads.h>

#include <Windows.h>

#include "awint.hpp"

namespace {
    using _Thrd_start_t = int (*)(void*);

    struct _Thrd_binder { // bind function pointer and data to pass to thread entry point
        _Thrd_start_t func;
        void* data;
        _Cnd_t* cond;
        _Mtx_t* mtx;
        int* started;
    };

    using _Thrd_callback_t = unsigned int(__stdcall*)(void*);

    unsigned int __stdcall _Thrd_runner(void* d) { // call thread function
        _Thrd_binder b = *static_cast<_Thrd_binder*>(d);
        _Mtx_lock(*b.mtx);
        *b.started = 1;
        _Cnd_signal(*b.cond);
        _Mtx_unlock(*b.mtx);
        const unsigned int res = b.func(b.data);
        _Cnd_do_broadcast_at_thread_exit();
        return res;
    }
} // unnamed namespace

_EXTERN_C

// TRANSITION, ABI: _Thrd_exit() is preserved for binary compatibility
[[noreturn]] _CRTIMP2_PURE void _Thrd_exit(int res) { // terminate execution of calling thread
    _endthreadex(res);
}

// TRANSITION, ABI: _Thrd_start() is preserved for binary compatibility
_CRTIMP2_PURE int _Thrd_start(_Thrd_t* thr, _Thrd_callback_t func, void* b) { // start a thread
    thr->_Hnd = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, func, b, 0, &thr->_Id));
    return thr->_Hnd == nullptr ? _Thrd_error : _Thrd_success;
}

int _Thrd_join(_Thrd_t thr, int* code) { // returns when thread terminates
    if (WaitForSingleObjectEx(thr._Hnd, INFINITE, FALSE) == WAIT_FAILED) {
        return _Thrd_error;
    }

    if (code) { // TRANSITION, ABI: code is preserved for binary compatibility
        unsigned long res;
        if (!GetExitCodeThread(thr._Hnd, &res)) {
            return _Thrd_error;
        }
        *code = static_cast<int>(res);
    }

    return CloseHandle(thr._Hnd) ? _Thrd_success : _Thrd_error;
}

int _Thrd_detach(_Thrd_t thr) { // tell OS to release thread's resources when it terminates
    return CloseHandle(thr._Hnd) ? _Thrd_success : _Thrd_error;
}

void _Thrd_sleep(const _timespec64* xt) { // suspend thread until time xt
    _timespec64 now;
    _Timespec64_get_sys(&now);
    do { // sleep and check time
        Sleep(_Xtime_diff_to_millis2(xt, &now));
        _Timespec64_get_sys(&now);
    } while (now.tv_sec < xt->tv_sec || now.tv_sec == xt->tv_sec && now.tv_nsec < xt->tv_nsec);
}

void _Thrd_yield() { // surrender remainder of timeslice
    SwitchToThread();
}

// TRANSITION, ABI: _Thrd_equal() is preserved for binary compatibility
_CRTIMP2_PURE int _Thrd_equal(_Thrd_t thr0, _Thrd_t thr1) { // return 1 if thr0 and thr1 identify same thread
    return thr0._Id == thr1._Id;
}

// TRANSITION, ABI: _Thrd_current() is preserved for binary compatibility
_CRTIMP2_PURE _Thrd_t _Thrd_current() { // return _Thrd_t identifying current thread
    _Thrd_t result;
    result._Hnd = nullptr;
    result._Id  = GetCurrentThreadId();
    return result;
}

_Thrd_id_t _Thrd_id() { // return unique id for current thread
    return GetCurrentThreadId();
}

unsigned int _Thrd_hardware_concurrency() { // return number of processors
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

// TRANSITION, ABI: _Thrd_create() is preserved for binary compatibility
_CRTIMP2_PURE int _Thrd_create(_Thrd_t* thr, _Thrd_start_t func, void* d) { // create thread
    int res;
    _Thrd_binder b;
    int started = 0;
    _Cnd_t cond;
    _Mtx_t mtx;
    _Cnd_init(&cond);
    _Mtx_init(&mtx, _Mtx_plain);
    b.func    = func;
    b.data    = d;
    b.cond    = &cond;
    b.mtx     = &mtx;
    b.started = &started;
    _Mtx_lock(mtx);
    if ((res = _Thrd_start(thr, _Thrd_runner, &b)) == _Thrd_success) { // wait for handshake
        while (!started) {
            _Cnd_wait(cond, mtx);
        }
    }
    _Mtx_unlock(mtx);
    _Cnd_destroy(cond);
    _Mtx_destroy(mtx);
    return res;
}

_END_EXTERN_C

/*
 * This file is derived from software bearing the following
 * restrictions:
 *
 * (c) Copyright William E. Kempf 2001
 *
 * Permission to use, copy, modify, distribute and sell this
 * software and its documentation for any purpose is hereby
 * granted without fee, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation. William E. Kempf makes no representations
 * about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */
