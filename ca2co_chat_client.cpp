//
// chat_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#pragma warning(push)
#pragma warning(disable : 4242)
#include <boost/asio.hpp>
#pragma warning(pop)
#include <ca2co/continuation.hpp>

#include "chat_message.hpp"

using boost::asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

class chat_client {
 public:
  chat_client(boost::asio::io_context& io_context)
      : io_context_(io_context), socket_(io_context) {}

  ca2co::continuation<> connect(const tcp::resolver::results_type& endpoints) {
    auto co_connect_result = co_await co_connect(endpoints);
    do_read_header();
  }

  void write(const chat_message& msg) {
    boost::asio::post(io_context_, [this, msg]() {
      bool write_in_progress = !write_msgs_.empty();
      write_msgs_.push_back(msg);
      if (!write_in_progress) {
        do_write();
      }
    });
  }

  void close() {
    boost::asio::post(io_context_, [this]() { socket_.close(); });
  }

 private:
  struct co_connect_result {
    boost::system::error_code ec;
    tcp::endpoint ep;
  };
  ca2co::continuation<co_connect_result> co_connect(
      const tcp::resolver::results_type& endpoints) {
    co_return co_await ca2co::await_callback_async<co_connect_result>(
        [&](std::function<void(co_connect_result)> const& handler) noexcept {
          boost::asio::async_connect(
              socket_, endpoints,
              [handler](boost::system::error_code ec, tcp::endpoint ep) {
                handler(co_connect_result{ec, ep});
              });
        });
  }

  void do_read_header() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec && read_msg_.decode_header()) {
            do_read_body();
          } else {
            socket_.close();
          }
        });
  }

  void do_read_body() {
    boost::asio::async_read(
        socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << "\n";
            do_read_header();
          } else {
            socket_.close();
          }
        });
  }

  void do_write() {
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_msgs_.front().data(),
                            write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
              do_write();
            }
          } else {
            socket_.close();
          }
        });
  }

 private:
  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
};

int main(int argc, char* argv[]) {
  try {
    //if (argc != 3) {
    //  std::cerr << "Usage: chat_client <host> <port>\n";
    //  return 1;
    //}

    boost::asio::io_context io_context;

    tcp::resolver resolver(io_context);
     //auto endpoints = resolver.resolve(argv[1], argv[2]);
    auto endpoints = resolver.resolve("localhost", "4400");
    chat_client c(io_context);
    ca2co::dont_await(
        [&] -> ca2co::continuation<> { co_await c.connect(endpoints); }());

    std::thread t([&io_context]() { io_context.run(); });

    char line[chat_message::max_body_length + 1];
    while (std::cin.getline(line, chat_message::max_body_length + 1)) {
      chat_message msg;
      msg.body_length(std::strlen(line));
      std::memcpy(msg.body(), line, msg.body_length());
      msg.encode_header();
      c.write(msg);
    }

    c.close();
    t.join();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}