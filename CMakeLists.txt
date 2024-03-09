cmake_minimum_required(VERSION 3.27)
project(DotNetFileParser)

set(CMAKE_CXX_STANDARD 17)

add_executable(DotNetFileParser
        main.cpp
        metadata.cpp
        stream.cpp
        models.cpp)
