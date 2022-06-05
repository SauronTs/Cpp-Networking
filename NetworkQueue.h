#pragma once

#include "NetworkCommon.h"

namespace TS
{
	namespace net
	{
		// This Queue must be threadsafe!
		template <typename A>
		class Queue {
		public:
			Queue() = default;
			Queue (const Queue<A>& q) = delete;
			Queue<A>& operator=(const Queue<A>& q) = delete;
			virtual ~Queue() { clear(); }

			A& front() {
				std::scoped_lock lock(mutex);
				return deque.front();
			}

			A& back() {
				std::scoped_lock lock(mutex);
				return deque.back();
			}

			void push_back(const A& data) {
				std::scoped_lock lock(mutex);
				deque.emplace_back(std::move(data));

				std::unique_lock<std::mutex> waitLock(waitMutex);
				wait.notify_one();
			}

			void push_front(const A& data) {
				std::scoped_lock lock(mutex);
				deque.emplace_front(std::move(data));

				std::unique_lock<std::mutex> waitLock(waitMutex);
				wait.notify_one();
			}

			bool empty() {
				std::scoped_lock lock(mutex);
				return deque.empty();
			}

			std::size_t size() {
				std::scoped_lock lock(mutex);
				return deque.size();
			}

			void clear() {
				std::scoped_lock lock(mutex);
				deque.clear();
			}

			A pop_front() {
				std::scoped_lock lock(mutex);
				A a(std::move(deque.front()));
				deque.pop_front();
				return a;
			}

			A pop_back() {
				std::scoped_lock lock(mutex);
				A a(std::move(deque.back()));
				deque.pop_back();
				return a;
			}

			void waitForMessage() {
				while (empty()) {
					std::unique_lock<std::mutex> lock(waitMutex);
					wait.wait(lock);
				}
			}

		protected:
			std::mutex mutex;
			std::deque<A> deque;
			std::condition_variable wait;
			std::mutex waitMutex;
		};
	}
}