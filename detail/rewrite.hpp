#include "header.hpp"
#include <type_traits>
#include <utility>

#define METHOD_NAME0 frob
#define SIGNATURE0 int(double)

class interface__3 : ::interface_detail::interface_tag
{
private:
    using interface = interface__3;
    friend auto& _fetch_obj(interface& i, ::interface_detail::interface_tag)
    {
        return i._obj;
    }
    friend auto& _fetch_vtable(interface& i, ::interface_detail::interface_tag)
    {
        return i._vtable;
    }
    friend auto& _fetch_thunk(interface& i, ::interface_detail::interface_tag)
    {
        return i._thunk;
    }
    friend auto _fetch_frob(interface& i, ::interface_detail::interface_tag)
    {
        return ::std::get<0>(i._vtable);
    }

    template <typename T>
    struct _call_frob
    {
        template <typename... Args>
        static decltype(auto) call(void* obj, Args&&... args)
        {
            return reinterpret_cast<T*>(obj)->METHOD_NAME0(
                ::std::forward<Args>(args)...);
        }
    };

public:
    interface__3() = default;
    interface__3(const interface& other)
    {
        _vtable = other._vtable;
        _thunk = other._thunk;
        _thunk->copy(_obj, other._obj);
    }
    interface__3(interface&& other) noexcept { swap(*this, other); }
    template <
        typename I,
        ::std::enable_if_t<
            ::interface_detail::is_interface_v<::std::decay_t<I>>, int> = 0>
    interface__3(I&& other)
    {
        if(!other)
            return;

        auto& thunk = _fetch_thunk(other, ::interface_detail::interface_tag{});
        auto& obj = _fetch_obj(other, ::interface_detail::interface_tag{});
        constexpr auto should_copy =
            ::std::is_lvalue_reference_v<I> || ::std::is_const_v<I>;

        _vtable = {
            _fetch_frob(other, ::interface_detail::interface_tag{}),
        };

        if constexpr(should_copy)
        {
            auto buf = ::std::make_unique<char[]>(thunk->size);
            thunk->copy(buf.get(), obj);
            _obj = ::std::launder(buf.get());
            buf.release();
            _thunk = thunk;
        }
        else
        {
            ::std::swap(_obj, obj);
            ::std::swap(_thunk, thunk);
            _fetch_vtable(other, ::interface_detail::interface_tag{}) = {};
        }
    }
    template <
        typename T,
        ::std::enable_if_t<
            !::interface_detail::is_interface_v<::std::decay_t<T>>, int> = 0>
    interface__3(T&& other)
    {
        using dT = ::std::decay_t<T>;
        static_assert(alignof(dT) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                      "overaligned types unsupported");
        static_assert(::std::is_copy_constructible_v<dT>,
                      "T required to be copy constructible");
        auto buf = ::std::make_unique<char[]>(sizeof(dT));
        _obj = new(buf.get()) dT{::std::forward<T>(other)};
        buf.release();
        _thunk = ::interface_detail::get_thunk<dT>();

        _vtable = {
            ::interface_detail::erasure_fn<SIGNATURE0, _call_frob<dT>>::value,
        };
    }
    ~interface__3()
    {
        if(_obj)
            _thunk->destroy(_obj);
        delete[] reinterpret_cast<char*>(_obj);
    }

    interface& operator=(const interface& rhs)
    {
        auto tmp = rhs;
        swap(*this, tmp);
        return *this;
    }
    interface& operator=(interface&& other) noexcept
    {
        auto tmp = ::std::move(other);
        swap(*this, tmp);
        return *this;
    }

    template <typename... Args>
    decltype(auto) METHOD_NAME0(Args&&... args)
    {
        return ::std::get<0>(_vtable)(_obj, ::std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept { return _obj; }

    friend void swap(interface& x, interface& y) noexcept
    {
        using ::std::swap;
        swap(x._vtable, y._vtable);
        swap(x._thunk, y._thunk);
        swap(x._obj, y._obj);
    }

private:
    using vtable_t = ::std::tuple<::interface_detail::erasure_fn_t<SIGNATURE0>*>;
    vtable_t _vtable = {};
    const ::interface_detail::thunk_t* _thunk = nullptr;
    void* _obj = nullptr;
};
