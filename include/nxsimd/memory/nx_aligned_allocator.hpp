//
// Copyright (c) 2016 Johan Mabille
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
//

#ifndef NX_ALIGNED_ALLOCATOR_HPP
#define NX_ALIGNED_ALLOCATOR_HPP

#include <cstddef>
#include "../config/nx_platform_config.hpp"
#include <algorithm>

#if defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
    #include <malloc.h>
#elif defined(__GNUC__)
    #include <mm_malloc.h>
    #if defined(NX_ALLOCA)
        #include <alloca.h>
    #endif
#else
    #include <stdlib.h>
#endif

namespace nxsimd
{

    template <class T, size_t Align>
    class aligned_allocator
    {

    public:

        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        static constexpr size_t alignment = Align;

        template <class U>
        struct rebind
        {
            using other = aligned_allocator<U, Align>;
        };

        aligned_allocator() noexcept;
        aligned_allocator(const aligned_allocator& rhs) noexcept;

        template <class U>
        aligned_allocator(const aligned_allocator<U, Align>& rhs) noexcept;

        ~aligned_allocator();

        pointer address(reference) noexcept;
        const_pointer address(const_reference) const noexcept;

        pointer allocate(size_type n, typename std::allocator<void>::const_pointer hint = 0);
        void deallocate(pointer p, size_type n);

        size_type size_max() const noexcept;

        template <class U, class... Args>
        void construct(U* p, Args&&... args);

        template <class U>
        void destroy(U* p);
    };

    template <class T1, size_t Align1, class T2, size_t Align2>
    bool operator==(const aligned_allocator<T1, Align1>& lhs,
                    const aligned_allocator<T2, Align2>& rhs) noexcept;

    template <class T1, size_t Align1, class T2, size_t Align2>
    bool operator!=(const aligned_allocator<T1, Align1>& lhs,
                    const aligned_allocator<T2, Align2>& rhs) noexcept;


    void* aligned_malloc(size_t size, size_t alignment);
    void aligned_free(void* ptr);

    template <class T>
    size_t get_alignment_offset(const T* p, size_t size, size_t block_size);


    /**************************************
     * aligned_allocator implementation
     **************************************/

    template <class T, size_t A>
    inline aligned_allocator<T, A>::aligned_allocator() noexcept
    {
    }

    template <class T, size_t A>
    inline aligned_allocator<T, A>::aligned_allocator(const aligned_allocator&) noexcept
    {
    }

    template <class T, size_t A>
    template <class U>
    inline aligned_allocator<T, A>::aligned_allocator(const aligned_allocator<U, A>&) noexcept
    {
    }

    template <class T, size_t A>
    inline aligned_allocator<T, A>::~aligned_allocator()
    {
    }

    template <class T, size_t A>
    inline auto
    aligned_allocator<T, A>::address(reference r) noexcept -> pointer
    {
        return &r;
    }

    template <class T, size_t A>
    inline auto
    aligned_allocator<T, A>::address(const_reference r) const noexcept -> const_pointer
    {
        return &r;
    }

    template <class T, size_t A>
    inline auto
    aligned_allocator<T, A>::allocate(size_type n,
            typename std::allocator<void>::const_pointer hint) -> pointer
    {
        pointer res = reinterpret_cast<pointer>(aligned_malloc(sizeof(T)*n, A));
        if(res == nullptr)
            throw std::bad_alloc();
        return res;
    }

    template <class T, size_t A>
    inline void aligned_allocator<T, A>::deallocate(pointer p, size_type)
    {
        aligned_free(p);
    }

    template <class T, size_t A>
    inline auto
    aligned_allocator<T, A>::size_max() const noexcept -> size_type
    {
        return size_type(-1) / sizeof(T);
    }

    template <class T, size_t A>
    template <class U, class... Args>
    inline void aligned_allocator<T, A>::construct(U* p, Args&&... args)
    {
        new ((void*)p) U(std::forward<Args>(args)...);
    }

    template <class T, size_t A>
    template <class U>
    inline void aligned_allocator<T, A>::destroy(U* p)
    {
        p->~U();
    }

    template <class T1, size_t A1, class T2, size_t A2>
    inline bool operator==(const aligned_allocator<T1, A1>& lhs,
                           const aligned_allocator<T2, A2>& rhs) noexcept
    {
        return lhs.alignment == rhs.alignment;
    }

    template <class T1, size_t A1, class T2, size_t A2>
    inline bool operator!=(const aligned_allocator<T1, A1>& lhs,
                           const aligned_allocator<T2, A2>& rhs) noexcept
    {
        return !(lhs == rhs);
    }


    /******************************************
     * aligned malloc / free implementation
     ******************************************/

    namespace detail
    {
        inline void* nx_aligned_malloc(size_t size, size_t alignment)
        {
            void* res = 0;
            void* ptr = malloc(size + alignment);
            if (ptr != 0)
            {
                res = reinterpret_cast<void*>(
                    (reinterpret_cast<size_t>(ptr) & ~(size_t(alignment - 1))) +
                    alignment);
                *(reinterpret_cast<void**>(res) - 1) = ptr;
            }
            return res;
        }

        inline void nx_aligned_free(void* ptr)
        {
            if (ptr != 0)
                free(*(reinterpret_cast<void**>(ptr) - 1));
        }
    }
    
    inline void* aligned_malloc(size_t size, size_t alignment)
    {
#if NX_MALLOC_ALREADY_ALIGNED
        return malloc(size);
#elif NX_HAS_MM_MALLOC
        return _mm_malloc(size, alignment);
#elif NX_HAS_POSIX_MEMALIGN
        void* res;
        const int failed = posix_memalign(&res, size, alignment);
        if (failed)
            res = 0;
        return res;
#elif(defined _MSC_VER)
        return _aligned_malloc(size, alignment);
#else
        return detail::nx_aligned_malloc(size, alignment);
#endif
    }

    inline void aligned_free(void* ptr)
    {
#if NX_MALLOC_ALREADY_ALIGNED
        free(ptr);
#elif NX_HAS_MM_MALLOC
        _mm_free(ptr);
#elif NX_HAS_POSIX_MEMALIGN
        free(ptr);
#elif defined(_MSC_VER)
        _aligned_free(ptr);
#else
        detail::nx_aligned_free(ptr);
#endif
    }

    template <class T>
    inline size_t get_alignment_offset(const T* p, size_t size, size_t block_size)
    {
        // size_t block_size = simd_traits<T>::size;
        if (block_size == 1)
        {
            // The simd_block consists of exactly one scalar so that all
            // elements of the array
            // are "well" aligned.
            return 0;
        }
        else if (size_t(p) & (sizeof(T) - 1))
        {
            // The array is not aligned to the size of a single element, so that
            // no element
            // of the array is well aligned
            return size;
        }
        else
        {
            size_t block_mask = block_size - 1;
            return std::min<size_t>(
                (block_size - ((size_t(p) / sizeof(T)) & block_mask)) & block_mask,
                size);
        }
    };
}

#endif

