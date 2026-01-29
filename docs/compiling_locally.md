# Compiling Locally

The ELFBSP code is fairly portable C++, and does not depend on any third-party libraries, save for standard libraries.
It requires at least C++20, and uses LLVM's clang++ as the main compiler.

Building should be fairly straight-forward on any Unix/Unix-like system, such as Linux, the BSDs, and even MacOS X.
With the main development dependencies being CMake, the C/C++ LLVM toolchain and standard library.
To build on Windows, it is recomended to use MinGW, as that is the preferred compiler toolchian for automated CI builds — however, Visual Studio also has support for [building projects with Clang](https://learn.microsoft.com/en-us/cpp/build/clang-support-msbuild).

On Debian/Ubuntu, for example, you will need the following packages:

- clang
- lld
- lld
- libc++-dev
- libc++abi-dev
- cmake
- make

```bash
sudo apt install -y clang lld lld libc++-dev libc++abi-dev cmake make
```

Make may be optionally replaced with Ninja. To build the program, type the following:

```bash
cmake -B build && make all -C build
```

To install ELFBSP system-wide, for which you will need root priveliges, do:

```bash
cmake -B build && sudo make install -C build
```
