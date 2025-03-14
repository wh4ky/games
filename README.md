# A collection of games I made.

## About

Simple games with a simple launcher for them. Everything is made in C and is centered around performance.  
Made specifically to be hosted on an SSH server.

## Building

To begin building the source files, well, you will need the source files!  
You can download the `.zip` from GitHub or `git clone` this project.

To build the program(s), you will need [CMake](https://cmake.org/).  
After that, just run the following :  

```bash
mkdir build
cd build
cmake -B . -S .. -DCMAKE_BUILD_TYPE=Release
make
cd ./out
```

After that, just launch the application with the `./launcher` command.  

## Bugs and debugging

When you have stumbeled onto a bug, recompile with the `-DCMAKE_BUILD_TYPE=Debug` flag and include the output of that command in a [new issue](https://github.com/wh4ky/games/issues/new).  
