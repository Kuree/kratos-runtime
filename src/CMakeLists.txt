add_library(kratos-runtime SHARED control.hh control.cc util.hh util.cc sim.cc sim.hh db.cc db.hh)

# target_link_libraries(kratos-runtime ${_GRPC_GRPCPP_UNSECURE} ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(kratos-runtime PRIVATE ../extern/cpp-httplib ../extern/fmt ../extern/sqlite_orm/include
        ../extern)
target_link_libraries(kratos-runtime fmt ${CMAKE_THREAD_LIBS_INIT} ${STATIC_FLAG} sqlite3 json11)