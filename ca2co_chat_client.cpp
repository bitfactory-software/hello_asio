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
  ~chat_client() { assert(false); }
  chat_client(boost::asio::io_context& io_context)
      : io_context_(io_context), socket_(io_context) {}

  ca2co::continuation<> connect(const tcp::resolver::results_type& endpoints) {
    auto [ec1, ep] = co_await co_connect(endpoints);
    if (ec1) {
      co_return;
    }

    while (true) {
      auto [ec2, read_length1] = co_await co_read_header();
      if (ec2 || !read_msg_.decode_header()) {
        socket_.close();
        co_return;
      }
      auto [ec3, read_length2] = co_await co_read_body();
      if (ec3) {
        socket_.close();
        co_return;
      }
      std::cout.write(read_msg_.body(), read_msg_.body_length());
      std::cout << "\n";
    }
  }

  void write(chat_message msg_) {
    boost::asio::post(io_context_, [=]() {
      ca2co::spawn([=] -> ca2co::continuation<> {
        auto msg = msg_;
        auto client = this;

        bool write_in_progress = !client->write_msgs_.empty();
        client->write_msgs_.push_back(msg);
        if (write_in_progress) co_return;

        do{
            auto [ec1, read_length1] = co_await client->co_write();
            if (ec1) {
              client->socket_.close();
              co_return;
            }

            client->write_msgs_.pop_front();
        } while(!client->write_msgs_.empty());
      }());
    });
  }

  void close() {
    boost::asio::post(io_context_, [this]() { socket_.close(); });
  }

  ca2co::continuation<boost::system::error_code, tcp::endpoint> co_connect(
      const tcp::resolver::results_type& endpoints) {
    return ca2co::callback_async<boost::system::error_code, tcp::endpoint>(
        [&](std::function<void(boost::system::error_code, tcp::endpoint)> const&
                handler) noexcept {
          boost::asio::async_connect(
              socket_, endpoints,
              [handler](boost::system::error_code ec, tcp::endpoint ep) {
                handler(ec, ep);
              });
        });
  }

  ca2co::continuation<boost::system::error_code, size_t> co_read_header() {
    return ca2co::callback_async<boost::system::error_code, size_t>(
        [&](std::function<void(boost::system::error_code, size_t)> const&
                handler) noexcept {
          boost::asio::async_read(
              socket_,
              boost::asio::buffer(read_msg_.data(),
                                  chat_message::header_length),
              [handler](boost::system::error_code ec, std::size_t length) {
                handler(ec, length);
              });
        });
  }

  ca2co::continuation<boost::system::error_code, size_t> co_read_body() {
    return ca2co::callback_async<boost::system::error_code, size_t>(
        [&](std::function<void(boost::system::error_code, size_t)> const&
                handler) noexcept {
          boost::asio::async_read(
              socket_,
              boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
              [handler](boost::system::error_code ec, std::size_t length) {
                handler(ec, length);
              });
        });
  }

  ca2co::continuation<boost::system::error_code, size_t> co_write() {
    return ca2co::callback_async<boost::system::error_code, size_t>(
        [&](std::function<void(boost::system::error_code, size_t)> const&
                handler) noexcept {
          boost::asio::async_write(
              socket_,
              boost::asio::buffer(write_msgs_.front().data(),
                                  write_msgs_.front().length()),
              [handler, this](boost::system::error_code ec,
                              std::size_t length) { handler(ec, length); });
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
    // if (argc != 3) {
    //   std::cerr << "Usage: chat_client <host> <port>\n";
    //   return 1;
    // }

    boost::asio::io_context io_context;

    tcp::resolver resolver(io_context);
    // auto endpoints = resolver.resolve(argv[1], argv[2]);
    auto endpoints = resolver.resolve("localhost", "4400");
    chat_client c(io_context);
    ca2co::spawn(
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