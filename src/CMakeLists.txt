add_library(kratos-runtime SHARED control.hh control.cc util.hh util.cc)

# target_link_libraries(kratos-runtime ${_GRPC_GRPCPP_UNSECURE} ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(kratos-runtime PRIVATE ../extern/cpp-httplib)