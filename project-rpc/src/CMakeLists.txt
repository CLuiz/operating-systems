cmake_minimum_required(VERSION 3.3)
project(ud923_project4)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    magickminify.c
    magickminify.h
    magickminify_test.c
    Makefile
    minify_via_rpc.c
    minifyjpeg.c
    minifyjpeg.h
    minifyjpeg.x
        minifyjpeg_clnt.c
    minifyjpeg_main.c
        minifyjpeg_svc.c
        minifyjpeg_xdr.c
    steque.c
    steque.h
    workload.txt)

add_executable(ud923_project4 ${SOURCE_FILES})