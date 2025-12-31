Compiling ELFBSP
===============

The ELFBSP code is fairly portable C++, and does not depend
on any third-party libraries. It requires at least C++11.
Both GNU g++ and clang++ are known to work.

Building should be fairly straight-forward on any Unix-like
system, such as Linux, the BSDs, and even MacOS X. With the
main development dependency being CMake

Linux / BSD
-----------

On Debian linux, you will need the following packages:

- g++
- binutils
- cmake
- make

To build the program, type the following:

```bash
cmake -B build && make all -C build
```

To install ELFBSP, for which you will need root priveliges, do:

```bash
cmake -B build && sudo make install -C build
```
