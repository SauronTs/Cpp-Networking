#pragma once

#include <iostream>
#include <vector>
#include <chrono>
#include <deque>
#include <mutex>
#include <memory>
#include <thread>

#define ASIO_STANDALONE
#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#endif
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>