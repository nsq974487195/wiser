project(grpc-cmake-example)
cmake_minimum_required(VERSION 3.2)

set(TCMALLOC_OPTS "-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")

set(PRODUCTION_OPTS "-std=c++11 -O3 -g -DNDEBUG -Wall -Wno-array-bounds")
set(INDEXING_OPTS   "-std=c++11 -O3 -g -Wall -Wno-array-bounds")
set(DEBUG_OPTS      "-std=c++11 -O0 -g -Wall -Wno-array-bounds")
set(DEV_OPTS        "-std=c++11 -O0 -g -Wall -Wno-array-bounds")

#SET(CMAKE_CXX_FLAGS "${PRODUCTION_OPTS} ${TCMALLOC_OPTS}")
SET(CMAKE_CXX_FLAGS ${PRODUCTION_OPTS})
#SET(CMAKE_CXX_FLAGS ${DEV_OPTS})
SET(CMAKE_C_FLAGS "-fPIC -std=c99 -O3 -msse4.1  -march=native -Wall -Wextra -pedantic -Wshadow")

set(GPROFILER profiler)
#set(CMAKE_CXX_COMPILER "/usr/lib/ccache/g++-5")

# GRPC and Protocol Buffers libraries location
list(APPEND CMAKE_PREFIX_PATH "/opt/grpc" "/opt/protobuf")

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)
find_package(Boost 1.5.4 COMPONENTS program_options REQUIRED )



set(PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/protos/qq.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/protos/posting_message.proto
)

SET(LITTLE_PACKED_INTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/LittleIntPacker)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
include_directories(${PROTO_SRC_DIR})
include_directories(${Boost_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${LITTLE_PACKED_INTS_DIR}/include)
INCLUDE_DIRECTORIES(${LITTLE_PACKED_INTS_DIR}/src)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

ADD_LIBRARY(little_packed_ints
    ${LITTLE_PACKED_INTS_DIR}/src/bitpacking32.c
    ${LITTLE_PACKED_INTS_DIR}/src/bmipacking32.c
    ${LITTLE_PACKED_INTS_DIR}/src/horizontalpacking32.c
    ${LITTLE_PACKED_INTS_DIR}/src/scpacking32.c
    ${LITTLE_PACKED_INTS_DIR}/src/turbobitpacking32.c
    ${LITTLE_PACKED_INTS_DIR}/src/util.c
)


protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})

LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libbloom/build)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libbloom)

set(ENGINE_SRCS 
    doc_store.cc
    posting.cc
    query_processing.cc
    query_pool.cc
    packed_value.cc
    types.cc
    qq_mem_engine.cc
    grpc_client_impl.cc
    grpc_server_impl.cc
    utils.cc
    engine_loader.cc
    vacuum_engine.cc
    flash_iterators.cc
    scoring.cc
    highlighter.cc
    )

set(QQ_LIBS
    boost_system
    boost_filesystem
    boost_locale
    lz4
    little_packed_ints
    bloom
    )

set(GRPC_LIBS
    gpr
    gRPC::grpc++_reflection
    )


set(GLOG_LIB glog ${GPROFILER} gflags)


#######################################
# GRPC Client and server
#######################################
add_executable(qq_server
    qq_server.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_link_libraries(qq_server
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)

# Building client
add_executable(qq_client
    ${CMAKE_CURRENT_SOURCE_DIR}/qq_client.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_link_libraries(qq_client
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)



#######################################
# Catch2 testing framework
#######################################
add_library( CatchMain OBJECT catch_main.cc catch.hpp )
add_executable( testmain 
    tests.cc 
    tests_2.cc
    tests_3.cc
    tests_4.cc
    tests_5.cc
    tests_6.cc
    tests_7.cc
    tests_8.cc
    tests_9.cc
    tests_10.cc
    tests_11.cc
    tests_12.cc
    tests_13.cc
    tests_14.cc
    tests_15.cc
    tests_16.cc
    tests_17.cc
    tests_18.cc
    test_helpers.cc
    grpc_client_impl.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
    $<TARGET_OBJECTS:CatchMain> catch.hpp )

target_link_libraries(testmain
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    ${GLOG_LIB}
)

add_executable( testlast
    tests_18.cc
    test_helpers.cc
    grpc_client_impl.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
    $<TARGET_OBJECTS:CatchMain> catch.hpp )

target_link_libraries(testlast
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    ${GLOG_LIB}
)




#######################################
# For benchmarks
#######################################

add_executable(packing_bench
    packing_bench.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
    test_helpers.cc
)

target_link_libraries(packing_bench
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)


add_executable(engine_bench
    engine_bench.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(engine_bench
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)


add_executable(create_bloom_store
    create_bloom_store.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(create_bloom_store
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)




add_executable(create_qq_mem_dump
    create_qq_mem_dump.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(create_qq_mem_dump
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)


add_executable(check_posting_list
    check_posting_list.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(check_posting_list
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)


add_executable(show_bloom_store_stats
    show_bloom_store_stats.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(show_bloom_store_stats
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)



add_executable(trie_bench
    trie_bench.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(trie_bench
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)

#target_compile_options(trie_bench PRIVATE "${TCMALLOC_OPTS}")


add_executable(convert_qq_to_vacuum
    convert_qq_to_vacuum.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(convert_qq_to_vacuum
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)

add_executable(grpc_benchmark
    grpc_client_impl.cc
    ${ENGINE_SRCS}
    grpc_bench.cc
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_link_libraries(grpc_benchmark
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)

add_executable(intersect_bench
    intersect_bench.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_link_libraries(intersect_bench
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)

add_executable(sorting_bench
    sorting_bench.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_link_libraries(sorting_bench
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)

add_executable(snippet_bench
    snippet_bench.cc
    ${ENGINE_SRCS}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
target_link_libraries(snippet_bench
    ${GRPC_LIBS}
    protobuf::libprotobuf
    ${QQ_LIBS}
    pthread
    ${GLOG_LIB}
)


