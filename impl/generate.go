package main

import (
	"flag"
	"fmt"
	"os"
	"text/template"
)

var header = `#include<memory>
#include<type_traits>
#include<cstddef>

// Implementaion namespace.
namespace interface_detail
{
    struct interface_tag {}; // As extra parameter for certain implementation functions to avoid namespace pollution.

    template<typename T>
    struct is_interface : std::is_base_of<interface_tag, T> {};

    template<typename T>
    inline static constexpr bool is_interface_v = is_interface<T>::value;

    // Base case factory for type erased method call.
    // Shouldn't be called. Working factories within the defined interface.
    struct nothing
    {
        template<typename... Args>
        void call(void*, Args&&...) {}
    };

    // erasure_fn is a traits class that handles void return types gracefully.
    template<typename Signature, typename Factory = nothing>
    struct erasure_fn;

    template<typename Ret, typename... Args, typename Factory>
    struct erasure_fn<Ret(Args...), Factory> : Factory
    {
        using type = Ret(void*, Args...);
        using return_type = Ret;
        static constexpr Ret value(void* p, Args... args)
        {
            if constexpr(std::is_void_v<Ret>)
                Factory::call(p, std::forward<Args>(args)...);
            else
                return Factory::call(p, std::forward<Args>(args)...);
        };
    };

    // Unified interface to access stored object.
    // Stored pointer signifies reference semantics.
    template<typename T>
    decltype(auto) as_object(void* p)
    {
        if constexpr(std::is_pointer_v<T>)
            return **static_cast<T*>(p);
        else
            return *static_cast<T*>(p);
    }

    // Type erased special member functions.
    struct thunk
    {
        void (*copy)(void* dst, const void* src) = nullptr;
        void (*move)(void* dst, void* src) = nullptr;
        void (*destroy)(void* p) noexcept = nullptr;
        std::size_t size = 0;
    };

    // Address of t acts as RTTI.
    template<typename T>
    struct thunk_storage
    {
        inline static constexpr thunk t = {
            [](void* dst, const void* src) {
                new (dst) T{*static_cast<const T*>(src)};
            },
            [](void* dst, void* src) {
                new (dst) T{std::move(*static_cast<T*>(src))};
            },
            [](void* p) noexcept {
                static_cast<T*>(p)->~T();
            },
            sizeof(T)
        };
    };

    template<typename T>
    constexpr const thunk* get_thunk()
    {
        return &thunk_storage<T>::t;
    }
}

namespace adl_helper
{
    // Used only for ADL.
    template<typename T, typename I>
    T* target(I&&);
}

// For creating anonymous variables.
#define INTERFACE_CONCAT_DIRECT(x, y) x##y
#define INTERFACE_CONCAT(x, y) INTERFACE_CONCAT_DIRECT(x, y)
#define INTERFACE_APPEND_LINE(x) INTERFACE_CONCAT(x, __LINE__)

#ifdef INTERFACE_FOR_EXPOSITION_ONLY
// The following is used only as documentation to the implementation of interface.
// SIGNATURE and METHOD_NAME are the parameters passed in by the user.
// The multitudes of versions each have a different arity.

// Inherits from interface_tag for type traits is_interface.
class INTERFACE_APPEND_LINE(_interface) : ::interface_detail::interface_tag
{
    // Alias for both readability and for recursively defined functions:
    // user may provide a function signature including interface.
    using interface = INTERFACE_APPEND_LINE(_interface);

    // Used in dispatching function call.
    // Used in converting from one interface to another to bypass access level.
    // interface_tag used to avoid namespace pollution, however improbable.
    friend auto get_##METHOD_NAME0(const interface& i, ::interface_detail::interface_tag)
    {
        using std::get;
        return get<0>(i._vtable);
    }

    // Factory for type erased method call
    // Suffix used to avoid name collisions.
    template <typename T>
    struct METHOD_NAME0##_0_factory
    {
        template <typename... Args>
        static decltype(auto) call(void* p, Args&&... args)
        {
            return ::interface_detail::as_object<T>(p).METHOD_NAME0(::std::forward<Args>(args)...);
        }
    };

    // Used in converting from one interface to another to bypass access level.
    // interface_tag used to avoid namespace pollution, however improbable.
    friend auto fetch_ptr(const interface& i, ::interface_detail::interface_tag) { return i._ptr; }

    // Used in converting from one interface to another to bypass access level.
    // interface_tag used to avoid namespace pollution, however improbable.
    friend auto&& fetch_thunk(const interface& i, ::interface_detail::interface_tag)
    {
        return i._t;
    }

  public:
    INTERFACE_APPEND_LINE(_interface)() = default;
    INTERFACE_APPEND_LINE(_interface)(interface&& other) { swap(*this, other); }

    // SFINAE on whether argument is an interface.
    // This is both the copy constructor and the converting constructor from other superset
    // interfaces.
    template <typename I,
              ::std::enable_if_t<::interface_detail::is_interface_v<::std::decay_t<I>>>* = nullptr>
    INTERFACE_APPEND_LINE(_interface)
    (I&& i)
    {
        if(!i)
            return;

        auto p = fetch_ptr(i, ::interface_detail::interface_tag{});
        auto t = fetch_thunk(i, ::interface_detail::interface_tag{});

        // Exception safe buffer allocation.
        auto buf = ::std::unique_ptr<::std::byte[]>(new ::std::byte[t->size]);

        if constexpr(::std::is_lvalue_reference_v<I> || ::std::is_const_v<I>)
            t->copy(buf.get(), p);
        else
            t->move(buf.get(), p);

        // Avoid [basic.life]/8 where original pointer cannot be used to refer to the newly
        // constructed object.
        _ptr = ::std::launder(buf.get());

        buf.release();
        _t = t;

        // Magic here. Constructs _vtable by name at compile time.
        // This is the reason why we can't use polymorphic classes as in std::function.
        _vtable = {
            get_##METHOD_NAME0(i, ::interface_detail::interface_tag{}),
        };
    }

    // SFINAE on whether argument is an interface.
    // This is the conversion from any type to an interface.
    template <typename T,
              ::std::enable_if_t<!::interface_detail::is_interface_v<::std::decay_t<T>>>* = nullptr>
    INTERFACE_APPEND_LINE(_interface)
    (T&& t)
    {
        using U = ::std::decay_t<T>;
        static_assert(alignof(U) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);

        // Exception safe buffer allocation.
        auto buf = ::std::unique_ptr<::std::byte[]>(new ::std::byte[sizeof(U)]);
        _ptr = new (buf.get()) U{::std::forward<T>(t)};
        buf.release();
        _t = ::interface_detail::get_thunk<U>();

        // Constructs _vtable by name at compile time.
        // erasure_fn is a unified interface to the method.
        // This can be done with polymorphic classes instead, but the previous constructor can't.
        _vtable = {
            ::interface_detail::erasure_fn<SIGNATURE0, METHOD_NAME0##_0_factory<U>>::value,
        };
    }

    ~INTERFACE_APPEND_LINE(_interface)()
    {
        if(_ptr)
            _t->destroy(_ptr);
        delete[] reinterpret_cast<::std::byte*>(_ptr);
    }

    interface& operator=(interface other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    template <typename... Args>
    decltype(auto) METHOD_NAME0(Args&&... args)
    {
        // Dispatches to type erased method call.
        return get_##METHOD_NAME0(*this, ::interface_detail::interface_tag{})(
            _ptr, ::std::forward<Args>(args)...);
    }

    // Fetches underlying type if thunk* matches, which serves as RTTI.
    template <typename T>
    friend T* target(interface&& i)
    {
        if(i._t == ::interface_detail::get_thunk<T>())
            return reinterpret_cast<T*>(i._ptr);
        else
            return nullptr;
    }
    template <typename T>
    friend T* target(interface& i)
    {
        if(i._t == ::interface_detail::get_thunk<T>())
            return reinterpret_cast<T*>(i._ptr);
        else
            return nullptr;
    }
    template <typename T>
    friend const T* target(const interface& i)
    {
        if(i._t == ::interface_detail::get_thunk<T>())
            return reinterpret_cast<T*>(i._ptr);
        else
            return nullptr;
    }

    operator bool() const { return _ptr; }
    friend void swap(interface& x, interface& y) noexcept
    {
        using ::std::swap;
        swap(x._ptr, y._ptr);
        swap(x._t, y._t);
        swap(x._vtable, y._vtable);
    }

  private:
    template <typename T>
    using erasure_fn_t = typename ::interface_detail::erasure_fn<T>::type;
    using vtable_t = ::std::tuple<erasure_fn_t<SIGNATURE0>*>;

    void* _ptr = nullptr;
    const ::interface_detail::thunk* _t = nullptr;
    vtable_t _vtable = {};
}

#endif // INTERFACE_FOR_EXPOSITION_ONLY

// The following is the actual implementaion for interface.
`

var interface_str = `{{define "macro args"}}
    {{- range $k, $v := . -}}
        {{if $k}}, {{end -}}
        SIGNATURE{{$v}}, METHOD_NAME{{$v -}}
    {{end}}
{{- end}}
{{- define "vtable funcs"}}
    {{- range $k, $v := . -}}
        {{if $k}}, {{end -}}
        erasure_fn_t<SIGNATURE{{$v -}}>*
    {{- end}}
{{- end}}
#define INTERFACE_{{len .}}({{template "macro args" .}})\
class INTERFACE_APPEND_LINE(_interface) : ::interface_detail::interface_tag\
{\
    using interface = INTERFACE_APPEND_LINE(_interface);\
\
    {{- range .}}
    friend auto get_##METHOD_NAME{{.}}(const interface& i, ::interface_detail::interface_tag)\
    {\
        using std::get;\
        return get<{{.}}>(i._vtable);\
    }\
\
    template<typename T>\
    struct METHOD_NAME{{.}}##_{{.}}_factory\
    {\
        template<typename... Args>\
        static decltype(auto) call(void* p, Args&&... args)\
        {\
            return ::interface_detail::as_object<T>(p).METHOD_NAME{{.}}(::std::forward<Args>(args)...);\
        }\
    };\
    {{- end}}
\
    friend auto fetch_ptr(const interface& i, ::interface_detail::interface_tag)\
    {\
        return i._ptr;\
    }\
\
    friend auto&& fetch_thunk(const interface& i, ::interface_detail::interface_tag)\
    {\
        return i._t;\
    }\
\
public:\
    INTERFACE_APPEND_LINE(_interface)() = default;\
    INTERFACE_APPEND_LINE(_interface)(interface&& other) { swap(*this, other); }\
    template<typename I, ::std::enable_if_t<::interface_detail::is_interface_v<::std::decay_t<I>>>* = nullptr>\
    INTERFACE_APPEND_LINE(_interface)(I&& i)\
    {\
        if(!i)\
            return;\
\
        auto p = fetch_ptr(i, ::interface_detail::interface_tag{});\
        auto t = fetch_thunk(i, ::interface_detail::interface_tag{});\
        auto buf = ::std::unique_ptr<::std::byte[]>(new ::std::byte[t->size]);\
        if constexpr(::std::is_lvalue_reference_v<I> || ::std::is_const_v<I>)\
            t->copy(buf.get(), p);\
        else\
            t->move(buf.get(), p);\
        _ptr = ::std::launder(buf.get());\
        buf.release();\
        _t = t;\
        _vtable = {\
            {{- range .}}
            get_##METHOD_NAME{{.}}(i, ::interface_detail::interface_tag{}),\
            {{- end}}
        };\
    }\
    template <typename T, ::std::enable_if_t<!::interface_detail::is_interface_v<::std::decay_t<T>>>* = nullptr>\
    INTERFACE_APPEND_LINE(_interface)(T&& t)\
    {\
        using U = ::std::decay_t<T>;\
        static_assert(alignof(U) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);\
        auto buf = ::std::unique_ptr<::std::byte[]>(new ::std::byte[sizeof(U)]);\
        _ptr = new (buf.get()) U{::std::forward<T>(t)};\
        buf.release();\
        _t = ::interface_detail::get_thunk<U>();\
\
        _vtable = {\
            {{- range .}}
            ::interface_detail::erasure_fn<SIGNATURE{{.}}, METHOD_NAME{{.}}##_{{.}}_factory<U>>::value,\
            {{- end}}
        };\
    }\
\
    ~INTERFACE_APPEND_LINE(_interface)()\
    {\
        if(_ptr)\
            _t->destroy(_ptr);\
        delete[] reinterpret_cast<::std::byte*>(_ptr);\
    }\
\
    interface& operator=(interface other) noexcept\
    {\
        swap(*this, other);\
        return *this;\
    }\
\
    {{- range .}}
    template<typename... Args>\
    decltype(auto) METHOD_NAME{{.}}(Args&&... args)\
    {\
        return get_##METHOD_NAME{{.}}(*this, ::interface_detail::interface_tag{})(_ptr, ::std::forward<Args>(args)...);\
    }\
    {{- end}}
\
    template<typename T>\
    friend T* target(interface&& i)\
    {\
        if(i._t == ::interface_detail::get_thunk<T>())\
            return reinterpret_cast<T*>(i._ptr);\
        else\
            return nullptr;\
    }\
    template<typename T>\
    friend T* target(interface& i)\
    {\
        if(i._t == ::interface_detail::get_thunk<T>())\
            return reinterpret_cast<T*>(i._ptr);\
        else\
            return nullptr;\
    }\
    template<typename T>\
    friend const T* target(const interface& i)\
    {\
        if(i._t == ::interface_detail::get_thunk<T>())\
            return reinterpret_cast<T*>(i._ptr);\
        else\
            return nullptr;\
    }\
\
    operator bool() const { return _ptr; }\
    friend void swap(interface& x, interface& y) noexcept\
    {\
        using ::std::swap;\
        swap(x._ptr, y._ptr);\
        swap(x._t, y._t);\
        swap(x._vtable, y._vtable);\
    }\
\
private:\
    template<typename T>\
    using erasure_fn_t = typename ::interface_detail::erasure_fn<T>::type;\
    using vtable_t = ::std::tuple<{{template "vtable funcs" .}}>;\
\
    void* _ptr = nullptr;\
    const ::interface_detail::thunk* _t = nullptr;\
    vtable_t _vtable = {};\
}
`

var footer = `{{define "dash"}}
    {{- range $k, $v := . -}}
        {{if $k}}, {{end -}}
        _{{.}}a, _{{.}}{{"b" -}}
    {{end}}
{{- end}}
{{define "name dash"}}
    {{- range $k, $v := . -}}
        {{if $k}}, {{end -}}
        INTERFACE_{{.}}, _{{. -}}
    {{end}}
{{- end}}
// Overloaded macros through __VA_ARGS__ hacking.
// Selects implementation by argument count.
#define GET_INTERFACE_FROM({{template "dash" .}}, x, ...) x
#define INTERFACE(...)\
GET_INTERFACE_FROM(__VA_ARGS__, {{template "name dash" .}})(__VA_ARGS__)

`

var N = flag.Int("N", 8, "maximum number of methods in interface")

func main() {
	flag.Parse()

	fmt.Println(header)

	s := []int{}
	tmp := template.Must(template.New("").Parse(interface_str))
	for i := 0; i < *N; i++ {
		s = append(s, i)
		tmp.Execute(os.Stdout, s)
	}

	r := []int{}
	for i := range s {
		r = append(r, len(s)-i)
	}
	template.Must(template.New("").Parse(footer)).Execute(os.Stdout, r)
}
