add_library(kratos-runtime SHARED control.hh control.cc util.hh util.cc sim.cc sim.hh db.cc db.hh expr.cc expr.hh)

# target_link_libraries(kratos-runtime ${_GRPC_GRPCPP_UNSECURE} ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(kratos-runtime PRIVATE ../extern/cpp-httplib
        ../extern/fmt ../extern/sqlite_orm/include
        ../extern
        ../extern/sqlite/include)
target_link_libraries(kratos-runtime ${CMAKE_THREAD_LIBS_INIT} ${STATIC_FLAG} sqlite3 fmt json11
        stdc++fs)