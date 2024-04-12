// Overloaded macros through __VA_ARGS__ hacking.
// Selects implementation by argument count.
#define GET_INTERFACE_FROM(_8a, _8b, _7a, _7b, _6a, _6b, _5a, _5b, _4a, _4b,   \
                           _3a, _3b, _2a, _2b, _1a, _1b, x, ...)               \
    x
#define INTERFACE(...)                                                         \
    GET_INTERFACE_FROM(__VA_ARGS__, INTERFACE_8, _8, INTERFACE_7, _7,          \
                       INTERFACE_6, _6, INTERFACE_5, _5, INTERFACE_4, _4,      \
                       INTERFACE_3, _3, INTERFACE_2, _2, INTERFACE_1, _1)      \
    (__VA_ARGS__)
