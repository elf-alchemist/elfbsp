Compiling
---------

The ELFBSP code is fairly portable C++, and does not depend on any third-party libraries, sa e for .
It requires at least C++20. Both GNU g++ and LLVM clang++ are known to work.

Building should be fairly straight-forward on any Unix-like system, such as Linux, the BSDs, and even MacOS X.
With the main development dependency being CMake, the C/C++ GNU/LLVM toolchains and library standard.
To build on Windows, it is recomended to use MinGW, as that is the preferred compiler oolchian for automated CI builds.

On Debian Linux, for example, you will need the following packages:

- g++
- binutils
- cmake
- make

Make may be optionally replaced with Ninja. To build the program, type the following:

```bash
cmake -B build && make all -C build
```

To install ELFBSP, for which you will need root priveliges, do:

```bash
cmake -B build && sudo make install -C build
```
