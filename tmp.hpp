#include <cstddef>
#include <memory>
#include <type_traits>

namespace interface
{
    namespace detail
    {
        using std::size_t;
        // tag is used as an argument whenever a free function is an implementation detail.
        struct tag
        {
        };
        struct base
        {
        };
    } // namespace detail

    template <typename T>
    struct is_interface : std::is_base_of<detail::base, T>
    {
    };

    template <typename T>
    inline constexpr bool is_interface_v = is_interface<T>::value;
} // namespace interface

namespace interface::detail
{
    template <typename T>
    using enable_if_interface_t = std::enable_if_t<is_interface_v<std::decay_t<T>>, int>;
    template <typename T>
    using disable_if_interface_t = std::enable_if_t<!is_interface_v<std::decay_t<T>>, int>;

    template <typename>
    struct as_erased;

    template <typename R, typename... Args>
    struct as_erased<R(Args...)>
    {
        using type = R(void*, Args...);
    };

    template <typename T>
    using as_erased_t = typename as_erased<T>::type;

    struct thunk_t
    {
        void (*copy)(void* dst, const void* src);
        void (*destroy)(void* p) noexcept;
        std::size_t size;
    };

    template <typename T>
    inline constexpr auto thunk =
        thunk_t{[](void* dst, const void* src) { new(dst) T{*static_cast<const T*>(src)}; },
                [](void* p) noexcept { static_cast<T*>(p)->~T(); }, sizeof(T)};

    template <typename T>
    struct intptr_pair
    {
        static constexpr size_t free_bits = [] {
            int log2 = 0;
            for(size_t i = 1; i < alignof(T); i *= 2)
                log2++;
            return log2;
        }();
        static_assert(free_bits > 0, "Must have free_bits in pointer");
        static constexpr uintptr_t int_mask = (1 << free_bits) - 1;
        static constexpr uintptr_t ptr_mask = ~int_mask;

        intptr_pair() = default;
        intptr_pair(T* p) : value{(uintptr_t)p} {}

        intptr_pair& operator=(T* p)
        {
            value = (uintptr_t)p | int_value();
            return *this;
        }

        T* ptr_value() const { return reinterpret_cast<T*>(value & ptr_mask); }

        T* operator->() const { return ptr_value(); }

        [[nodiscard]] int int_value() const { return value & int_mask; }

        void int_value(int i) { value = (uintptr_t)ptr_value() | i; }

        std::uintptr_t value = 0;
    };

    template <typename I>
    auto as_refs(I&& i)
    {
        return std::forward_as_tuple(std::forward<I>(i)._objptr, std::forward<I>(i)._thunk,
                                     std::forward<I>(i)._vtable);
    }

    // raii_storage is unique_ptr for void* without the hassle.
    // raii_storage should only be used as local variables for exception safety.
    // raii_storage should never be members or passed around.
    // Deallocate memory acquired from raii_storage with raii_storage::deallocate.
    struct raii_storage
    {
        raii_storage() = default;
        raii_storage(size_t n) : ptr{operator new(n)} {}
        raii_storage(const raii_storage&) = delete;
        ~raii_storage() { deallocate(ptr); }

        void release() { ptr = nullptr; }

        static void deallocate(void* ptr) { operator delete(ptr); }

        void* ptr = nullptr;
    };
} // namespace interface::detail

class interface__ : ::interface::detail::base
{
    using interface = interface__;
    using byte = ::std::byte;

    template <typename I>
    friend auto ::interface::detail::as_refs(I&&);

    struct erased_METHOD_NAME
    {
        template <typename T, typename R, typename... Args>
        static auto make(R (*)(Args...))
        {
            return +[](void* p, Args... args) -> R {
                auto& obj = *static_cast<T*>(p);
                return R(obj.METHOD_NAME(::std::forward<Args>(args)...));
            };
        }
    };

  public:
    interface__() = default;
    interface__(interface&& other) noexcept { swap(*this, other); }
    interface__(const interface& other) { construct(other); }

    template <typename I, ::interface::detail::enable_if_interface_t<I> = 0>
    interface__(I&& other)
    {
        construct(::std::forward<I>(other));
    }

    template <typename T, ::interface::detail::disable_if_interface_t<T> = 0>
    interface__(T&& x)
    {
        using dT = ::std::decay_t<T>;
        if constexpr(::std::is_pointer_v<dT>)
        {
            _objptr = x;
        }
        else
        {
            ::interface::detail::raii_storage buf{sizeof(dT)};
            _objptr = new(buf.ptr) dT{std::forward<T>(x)};
            owns_object(*this, true, ::interface::detail::tag{});
            buf.ptr = nullptr;
        }

        using rdT = ::std::remove_pointer_t<dT>;
        _thunk = &::interface::detail::thunk<rdT>;

        using method = struct erased_METHOD_NAME;
        _vtable = {
            method::make<rdT>(::std::add_pointer_t<SIGNATURE>(nullptr)),
        };
    }

    ~interface__()
    {
        if(owns_object(*this, ::interface::detail::tag{}))
        {
            _thunk->destroy(_objptr);
            ::interface::detail::raii_storage::deallocate(_objptr);
        }
    }

    interface& operator=(const interface& other)
    {
        auto tmp = other;
        swap(*this, tmp);
        return *this;
    }
    interface& operator=(interface&& other) noexcept
    {
        auto tmp = ::std::move(other);
        swap(*this, tmp);
        return *this;
    }

    friend void swap(interface& x, interface& y) noexcept
    {
        using ::std::swap;
        swap(x._objptr, y._objptr);
        swap(x._thunk, y._thunk);
        swap(x._vtable, y._vtable);
    }

    template <typename T>
    friend T* target(interface& i) noexcept
    {
        if(i._thunk == &::interface::detail::thunk<T>)
            return static_cast<T*>(i._objptr);
        else
            return nullptr;
    }

    template <typename T>
    friend const T* target(const interface& i) noexcept
    {
        if(i._thunk == &::interface::detail::thunk<T>)
            return static_cast<const T*>(i._objptr);
        else
            return nullptr;
    }

    template <typename... Args>
    decltype(auto) METHOD_NAME(Args&&... args)
    {
        auto fn = get_METHOD_NAME(*this, ::interface::detail::tag{});
        return fn(_objptr, ::std::forward<Args>(args)...);
    }

  private:
    using thunk_ptr = ::interface::detail::intptr_pair<const ::interface::detail::thunk_t>;
    using vtable_t = ::std::tuple<::interface::detail::as_erased_t<SIGNATURE>*>;

    void* _objptr = nullptr;
    thunk_ptr _thunk = nullptr;
    vtable_t _vtable = {};

    template <typename I>
    void construct(I&& other)
    {
        auto [objptr, thunk, vtable] = ::interface::detail::as_refs(::std::forward<I>(other));
        if(!objptr)
            return;

        _vtable = {
            get_METHOD_NAME(other, ::interface::detail::tag{}),
        };

        if constexpr(::std::is_lvalue_reference_v<I> || ::std::is_const_v<I>)
        {
            if(owns_object(other, ::interface::detail::tag{}))
            {
                ::interface::detail::raii_storage buf{thunk->size};
                thunk->copy(buf.ptr, objptr);
                _objptr = buf.ptr;
                owns_object(*this, true, ::interface::detail::tag{});
                buf.ptr = nullptr;
            }
            else
            {
                _objptr = objptr;
            }
            _thunk = thunk;
        }
        else
        {
            using ::std::swap;
            swap(_objptr, objptr);
            swap(_thunk, thunk);
            vtable = {};
        }
    }

    [[nodiscard]] friend bool owns_object(const interface& i, ::interface::detail::tag)
    {
        return i._thunk.int_value();
    }

    friend void owns_object(interface& i, bool val, ::interface::detail::tag)
    {
        i._thunk.int_value(val);
    }

    friend auto get_METHOD_NAME(const interface& i, ::interface::detail::tag)
    {
        return ::std::get<0>(i._vtable);
    }
};
