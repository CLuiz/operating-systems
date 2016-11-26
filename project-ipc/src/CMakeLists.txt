cmake_minimum_required(VERSION 3.3)
project(ud923_project3)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        Part2/gfclient_download
        Part2/gfclient_measure
        Part2/gfserver.h
        Part2/gfserver.o
        Part2/handle_with_cache.c
        Part2/handle_with_curl.c
        Part2/handle_with_file.c
        Part2/locals.txt
        Part2/Makefile
        Part2/shm_channel.c
        Part2/shm_channel.h
        Part2/simplecache.c
        Part2/simplecache.h
        Part2/simplecached.c
        Part2/steque.c
        Part2/steque.h
        Part2/webproxy.c
        Part2/workload.txt)

add_executable(ud923_project3 ${SOURCE_FILES})