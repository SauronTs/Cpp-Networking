#pragma once

#include "NetworkCommon.h"
#include "NetworkMessage.h"
#include "NetworkQueue.h"

namespace TS
{
	namespace net
	{
		template <typename A>
		class Connection : public std::enable_shared_from_this<Connection<A>> {
		public:
			enum class Owner;

			Connection(Owner owner, asio::io_context& context, asio::ip::tcp::socket socket, Queue<OwnedMessage<A>>& incomingMessages) : m_context(context), m_socket(std::move(socket)), m_incomingMessages(incomingMessages), m_owner(owner) {
				m_clientId = 0;

				if (m_owner == Owner::Server) {
					m_handShakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
					m_handShakeCheck = scramble(m_handShakeOut);
				}
			}

			virtual ~Connection() {

			}

			bool connect(const asio::ip::tcp::endpoint& endpoint) {
				asio::error_code ec;
				m_socket.connect(endpoint, ec);

				if (!ec) {

					return true;
				}
				
				std::cout << "[Connection] - Error: " << ec.message() << std::endl;

				return false;
			}

			void connectToClient() {
				if (m_owner == Owner::Server) {
					if (m_socket.is_open()) {
						//readHeader();
						writeVerification();
						readVerification();
					}
				}
			}

			void connectToServer(const asio::ip::tcp::resolver::results_type& endpoint) {
				if (m_owner == Owner::Client) {
					asio::async_connect(m_socket, endpoint, [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
						if (!ec) {
							//readHeader();
							readVerification();
						}
					});
				}
			}

			bool disconnect() {
				if (isConnected()) {
					asio::post(m_context, [this]() {
						m_socket.close();
					});
				}
			}

			bool isConnected() const { return m_socket.is_open(); }

			void send(const Message<A>& msg) {
				asio::post(m_context, [this, msg]() {
					bool empty = m_outQueue.empty();
					m_outQueue.push_back(msg);

					if (empty) {
						writeHeader();
					}
				});
			}

			void setClientId(uint32_t id) { m_clientId = id; }
			uint32_t getClientId() const { return m_clientId; }

			enum class Owner {
				Server,
				Client
			};

		protected:
			asio::io_context& m_context;
			asio::ip::tcp::socket m_socket;

			// Messages to be sent
			Queue<Message<A>> m_outQueue {};

			// Incoming Messages are queued here.
			Queue<OwnedMessage<A>>& m_incomingMessages;
			Message<A> m_buffer {};

			Owner m_owner;

			uint32_t m_clientId;

			uint64_t m_handShakeOut = 0;
			uint64_t m_handShakeIn = 0;
			uint64_t m_handShakeCheck = 0;

		private:
			// Async
			void readHeader() {
				asio::async_read(m_socket, asio::buffer(&m_buffer.header, sizeof(MessageHeader<A>)), [this](std::error_code ec, std::size_t size) {
					if (!ec) {
						if (m_buffer.header.size > 0) {
							m_buffer.body.resize(m_buffer.header.size);
							readBody();
						}
						else {
							addToIncomingMessageQueue();
						}

						//std::cout << "[Connection] - Read header. Size: " << m_buffer.header.size << std::endl;
					}
					else {
						std::cerr << "[Server] - Reading header by client " << m_clientId << " failed: " << ec.message() << std::endl;
						m_socket.close();
					}
				});
			}

			// Async
			void readBody() {
				// Or body.size?
				asio::async_read(m_socket, asio::buffer(m_buffer.body.data(), m_buffer.body.size()), [this](std::error_code ec, std::size_t size) {
					if (!ec) {
						addToIncomingMessageQueue();

						//std::cout << "[Connection] - Read body." << std::endl;
					}
					else {
						std::cerr << "[Server] - Reading body by client " << m_clientId << " failed: " << ec.message() << std::endl;
						m_socket.close();
					}
				});
			}

			// Async
			void writeHeader() {
				asio::async_write(m_socket, asio::buffer(&m_outQueue.front().header, sizeof(MessageHeader<A>)), [this](std::error_code ec, std::size_t size) {
					if (!ec) {
						if (m_outQueue.front().body.size() > 0) {
							writeBody();
						}
						else {
							m_outQueue.pop_front();

							if (!m_outQueue.empty()) {
								writeHeader();
							}
						}

						//std::cout << "[Connection] - Wrote header." << std::endl;
					}
					else {
						std::cerr << "[Server] - Writing header by client " << m_clientId << " failed: " << ec.message() << std::endl;
						m_socket.close();
					}
				});
			}

			// Async
			void writeBody() {
				asio::async_write(m_socket, asio::buffer(m_outQueue.front().body.data(), m_outQueue.front().body.size()), [this](std::error_code ec, std::size_t size) {
					if (!ec) {
						m_outQueue.pop_front();

						if (!m_outQueue.empty()) {
							writeHeader();
						}

						//std::cout << "[Connection] - Wrote body." << std::endl;
					}
					else {
						std::cerr << "[Server] - Writing body by client " << m_clientId << " failed: " << ec.message() << std::endl;
						m_socket.close();
					}
				});
			}

			void addToIncomingMessageQueue() {
				if (m_owner == Owner::Server) {
					m_incomingMessages.push_back({ this->shared_from_this(), m_buffer });
				}
				else {
					m_incomingMessages.push_back({ nullptr, m_buffer });
				}

				readHeader();
			}

			uint64_t scramble(uint64_t data) {
				uint64_t output = data ^ 0xC001BEEFC0DECAFE;
				output = (output & 0xF0F0F0F0F0F0F0F0) >> 4 | (output & 0x0F0F0F0F0F0F0F0F) << 4;
				return output ^ 0x12345678FACEBEEF;
			}

			void writeVerification() {
				asio::async_write(m_socket, asio::buffer(&m_handShakeOut, sizeof(uint64_t)), [this](std::error_code ec, std::size_t size) {
					if (!ec) {
						if (m_owner == Owner::Client) {
							readHeader();
						}
					}
					else {
						m_socket.close();
					}
				});
			}

			void readVerification() {
				asio::async_read(m_socket, asio::buffer(&m_handShakeIn, sizeof(uint64_t)), [this](std::error_code ec, std::size_t size) {
					if (!ec) {
						if (m_owner == Owner::Client) {
							m_handShakeOut = scramble(m_handShakeIn);
							writeVerification();
						}
						else {
							if (m_handShakeIn == m_handShakeCheck) {

								readHeader();
							}
							else {
								m_socket.close();
							}
						}
					}
					else {
						m_socket.close();
					}
				});
			}
		};
	}
}