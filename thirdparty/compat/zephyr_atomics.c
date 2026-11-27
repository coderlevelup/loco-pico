/*
 * Copyright 2026 loco-pico contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * GCC __atomic builtin fallbacks for Armv6-M (RP2040 / Cortex-M0+).
 *
 * zenoh-pico's refcounting (built when Z_FEATURE_MULTI_THREAD=1) uses C11
 * <stdatomic.h>. On Cortex-M0+ the compiler cannot inline 4-byte atomics, so
 * it emits calls to the libatomic runtime helpers (__atomic_fetch_add_4 and
 * friends). The Zephyr SDK does not ship libatomic, and Zephyr's own C
 * implementation (lib/cpp/abi/cpp_atomics.c) provides only
 * __atomic_compare_exchange_* and only when C++ support is enabled.
 *
 * The missing helpers are reimplemented here on top of irq_lock(). On a single
 * core that is a valid full-barrier critical section: it disables interrupts
 * and therefore also stops another thread from being scheduled mid-operation,
 * which matches the strongest memory ordering (__ATOMIC_SEQ_CST).
 *
 * The file only compiles where Zephyr selects the generic C atomics
 * (CONFIG_ATOMIC_OPERATIONS_C), i.e. exactly the targets that need it.
 */

#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>

#if defined(CONFIG_ATOMIC_OPERATIONS_C)

#define DEFINE_ATOMIC_FETCH_ADD(n, type)                                     \
	type __atomic_fetch_add_##n(volatile void *ptr, type val, int memorder) \
	{                                                                    \
		unsigned int key = irq_lock();                               \
		volatile type *p = ptr;                                      \
		type old = *p;                                               \
		*p = (type)(old + val);                                      \
		irq_unlock(key);                                             \
		(void)memorder;                                              \
		return old;                                                  \
	}

#define DEFINE_ATOMIC_FETCH_SUB(n, type)                                     \
	type __atomic_fetch_sub_##n(volatile void *ptr, type val, int memorder) \
	{                                                                    \
		unsigned int key = irq_lock();                               \
		volatile type *p = ptr;                                      \
		type old = *p;                                               \
		*p = (type)(old - val);                                      \
		irq_unlock(key);                                             \
		(void)memorder;                                              \
		return old;                                                  \
	}

#define DEFINE_ATOMIC_EXCHANGE(n, type)                                     \
	type __atomic_exchange_##n(volatile void *ptr, type val, int memorder) \
	{                                                                   \
		unsigned int key = irq_lock();                              \
		volatile type *p = ptr;                                     \
		type old = *p;                                              \
		*p = val;                                                   \
		irq_unlock(key);                                            \
		(void)memorder;                                             \
		return old;                                                 \
	}

DEFINE_ATOMIC_FETCH_ADD(1, uint8_t)
DEFINE_ATOMIC_FETCH_ADD(2, uint16_t)
DEFINE_ATOMIC_FETCH_ADD(4, uint32_t)

DEFINE_ATOMIC_FETCH_SUB(1, uint8_t)
DEFINE_ATOMIC_FETCH_SUB(2, uint16_t)
DEFINE_ATOMIC_FETCH_SUB(4, uint32_t)

DEFINE_ATOMIC_EXCHANGE(1, uint8_t)
DEFINE_ATOMIC_EXCHANGE(2, uint16_t)
DEFINE_ATOMIC_EXCHANGE(4, uint32_t)

/*
 * Zephyr already supplies __atomic_compare_exchange_1/2/4 (in
 * lib/cpp/abi/cpp_atomics.c) whenever C++ support is enabled, so only define
 * them when that implementation is absent.
 */
#if !defined(CONFIG_CPP)

#define DEFINE_ATOMIC_COMPARE_EXCHANGE(n, type)                                              \
	bool __atomic_compare_exchange_##n(volatile void *ptr, void *expected, type desired, \
					   bool weak, int success, int failure)              \
	{                                                                                    \
		unsigned int key = irq_lock();                                               \
		volatile type *p = ptr;                                                      \
		type *e = expected;                                                          \
		bool ret;                                                                    \
                                                                                             \
		if (*p == *e) {                                                              \
			*p = desired;                                                        \
			ret = true;                                                          \
		} else {                                                                     \
			*e = *p;                                                             \
			ret = false;                                                         \
		}                                                                            \
		irq_unlock(key);                                                             \
		(void)weak;                                                                  \
		(void)success;                                                               \
		(void)failure;                                                               \
		return ret;                                                                  \
	}

DEFINE_ATOMIC_COMPARE_EXCHANGE(1, uint8_t)
DEFINE_ATOMIC_COMPARE_EXCHANGE(2, uint16_t)
DEFINE_ATOMIC_COMPARE_EXCHANGE(4, uint32_t)

#endif /* !CONFIG_CPP */

#endif /* CONFIG_ATOMIC_OPERATIONS_C */
