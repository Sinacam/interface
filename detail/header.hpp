#include <cstddef>
#include <memory>
#include <type_traits>

// Implementaion namespace.
namespace interface_detail
{
    struct interface_tag
    {
    };

    template <typename T>
    struct is_interface : std::is_base_of<interface_tag, T>
    {
    };

    template <typename T>
    inline static constexpr bool is_interface_v = is_interface<T>::value;

    // Base case factory for type erased method call.
    // Shouldn't be called. Working factories within the defined interface.
    struct nothing
    {
        template <typename... Args>
        void call(void*, Args&&...)
        {
        }
    };

    // erasure_fn is a traits class that handles void return types gracefully.
    template <typename Signature, typename Call = nothing>
    struct erasure_fn;

    template <typename Ret, typename... Args, typename Call>
    struct erasure_fn<Ret(Args...), Call>
    {
        using type = Ret(void*, Args...);
        using return_type = Ret;
        static constexpr Ret value(void* p, Args... args)
        {
            return Ret(Call::call(p, std::forward<Args>(args)...));
        };
    };

    template<typename Signature>
    using erasure_fn_t = typename erasure_fn<Signature>::type;

    // Unified interface to access stored object.
    // Stored pointer signifies reference semantics.
    template <typename T>
    decltype(auto) as_object(void* p)
    {
        if constexpr(std::is_pointer_v<T>)
            return **static_cast<T*>(p);
        else
            return *static_cast<T*>(p);
    }

    // Type erased special member functions.
    struct thunk_t
    {
        void (*copy)(void* dst, const void* src) = nullptr;
        void (*destroy)(void* p) noexcept = nullptr;
        std::size_t size = 0;
    };

    // Address of t acts as RTTI
    template <typename T>
    struct thunk_storage
    {
        inline static constexpr thunk_t t = {
            [](void* dst, const void* src)
            { new(dst) T{*static_cast<const T*>(src)}; },
            [](void* p) noexcept { static_cast<T*>(p)->~T(); }, sizeof(T)};
    };

    template <typename T>
    constexpr const thunk_t* get_thunk()
    {
        // Returns same thunk for all pointer types, used to determine whether
        // interface has reference semantics.
        if constexpr(std::is_pointer_v<T>)
            return &thunk_storage<void*>::t;
        else
            return &thunk_storage<T>::t;
    }

    // All pointer thunks are void* thunks.
    constexpr bool is_pointer_thunk(const thunk_t* t)
    {
        return t == get_thunk<void*>();
    }
} // namespace interface_detail

// For ADL purposes.
template <typename T, typename I>
void target(I&&, ::interface_detail::interface_tag);

// For creating anonymous variables.
#define INTERFACE_CONCAT_DIRECT(x, y) x##y
#define INTERFACE_CONCAT(x, y) INTERFACE_CONCAT_DIRECT(x, y)
#define INTERFACE_APPEND_LINE(x) INTERFACE_CONCAT(x, __LINE__)