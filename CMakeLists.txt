# CMakeList.txt : CMake project for hello_asio, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.31)


 include(FetchContent)
FetchContent_Declare(
    boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-1.86.0
)
FetchContent_Declare(
    cogoproject
    GIT_REPOSITORY https://github.com/bitfactory-software/ca2co.git
    GIT_TAG main
)

FetchContent_MakeAvailable(boost cogoproject)

project ("hello_asio")

add_executable(chat_server chat_server.cpp)
target_link_libraries(
  chat_server
  PRIVATE Boost::asio)
 set_property(TARGET chat_server PROPERTY CXX_STANDARD 23)

add_executable(chat_client chat_client.cpp)
target_link_libraries(
  chat_client
  PRIVATE Boost::asio)
  set_property(TARGET chat_client PROPERTY CXX_STANDARD 23)

add_executable(ca2co_chat_client ca2co_chat_client.cpp)
target_link_libraries(
  ca2co_chat_client
  PRIVATE cogoproject::ca2co
          Boost::asio)
  set_property(TARGET ca2co_chat_client PROPERTY CXX_STANDARD 23)

# TODO: Add tests and install targets if needed.
