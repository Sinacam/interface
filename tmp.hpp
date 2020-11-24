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

    // as_erased adds an additional void* as the first argument of function types.
    template <typename>
    struct as_erased;

    template <typename R, typename... Args>
    struct as_erased<R(Args...)>
    {
        using type = R(void*, Args...);
    };

    template <typename T>
    using as_erased_t = typename as_erased<T>::type;

    // thunk_t stores the copy constructor, destructor and size of a type in a type erased manner.
    struct thunk_t
    {
        void (*copy)(void* dst, const void* src);
        void (*destroy)(void* p) noexcept;
        std::size_t size;
    };

    // thunk is a thunk_t with the fields filled in.
    // &thunk is unique for each type, which is used as runtime type identity.
    // clang-format off
    template <typename T>
    inline constexpr auto thunk = thunk_t{
        [](void* dst, const void* src) { new(dst) T{*static_cast<const T*>(src)}; },
        [](void* p) noexcept { static_cast<T*>(p)->~T(); },
        sizeof(T)
    };
    // clang-format on

    // intptr_pair is a pointer-like type which steals the bottom bits as a small integer.
    // intptr_pair makes no assumption of the platform except that alignment of function
    // pointers is non-zero.
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

    // as_refs returns an interface as a tuple of references to the members.
    // The correct way to use structured bindings is
    //      auto [objptr, thunk, vtable] = as_refs(i);
    // They are references, not copied values.
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

// interface__ is "template" for the type created by INTERFACE.
// interface__ will be replaced by an unique name.
// Every appearance of SIGNATURE and METHOD_NAME will be replaced by multiple copies
// of the same surrounding code, where each copy corresponds to the passed in
// signatures and method names.
// To avoid naming conflicts, there are only special member functions, member operators
// and type aliases. All other functionality is provided through friend functions with
// detail::tag as the last parameter to make them "private".
class interface__ : ::interface::detail::base
{
    // This alias enables the keyword-like capability for self reference.
    using interface = interface__;

    template <typename I>
    friend auto ::interface::detail::as_refs(I&&);

    // erased_METHOD_NAME::make deduces the signature and returns the type erased version
    // of the method.
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
    interface__(const interface& other) { construct(*this, other, ::interface::detail::tag{}); }

    // converting constructor for all (non-strict) subset interfaces.
    template <typename I, ::interface::detail::enable_if_interface_t<I> = 0>
    interface__(I&& other) noexcept(!::std::is_lvalue_reference_v<I> && !::std::is_const_v<I>)
    {
        construct(*this, ::std::forward<I>(other), ::interface::detail::tag{});
    }

    // converting constructor for any type satisfying the interface.
    template <typename T, ::interface::detail::disable_if_interface_t<T> = 0>
    interface__(T&& x) noexcept(::std::is_pointer_v<::std::decay_t<T>>)
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
            buf.release();
        }

        using rdT = ::std::remove_pointer_t<dT>;
        _thunk = &::interface::detail::thunk<rdT>;

        // Alias elaborated class name to avoid naming conflict.
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
    friend void construct(interface& self, I&& other,
                          ::interface::detail::tag) noexcept(!::std::is_lvalue_reference_v<I> &&
                                                             !::std::is_const_v<I>)
    {
        auto [objptr, thunk, vtable] = ::interface::detail::as_refs(::std::forward<I>(other));
        if(!objptr)
            return;

        self._vtable = {
            get_METHOD_NAME(other, ::interface::detail::tag{}),
        };

        if constexpr(::std::is_lvalue_reference_v<I> || ::std::is_const_v<I>)
        {
            if(owns_object(other, ::interface::detail::tag{}))
            {
                ::interface::detail::raii_storage buf{thunk->size};
                thunk->copy(buf.ptr, objptr);
                self._objptr = buf.ptr;
                owns_object(self, true, ::interface::detail::tag{});
                buf.release();
            }
            else
            {
                self._objptr = objptr;
            }
            self._thunk = thunk;
        }
        else
        {
            using ::std::swap;
            swap(self._objptr, objptr);
            swap(self._thunk, thunk);
            vtable = {};
        }
    }

    [[nodiscard]] friend bool owns_object(const interface& i, ::interface::detail::tag) noexcept
    {
        return i._thunk.int_value();
    }

    friend void owns_object(interface& i, bool val, ::interface::detail::tag) noexcept
    {
        i._thunk.int_value(val);
    }

    friend auto get_METHOD_NAME(const interface& i, ::interface::detail::tag) noexcept
    {
        return ::std::get<0>(i._vtable);
    }
};
