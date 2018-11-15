To override default maximum values of 8 methods in an interface,
build and run generate.go with flag -N=new_maximum

eg For maximum of 16 methods in Interface

go build
./impl -N=16 > interface.hpp

There is no runtime penalty for doing so, but source file size is O(N^2).

Built and tested for go1.9.2