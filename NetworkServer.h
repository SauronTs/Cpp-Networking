#pragma once

#include "NetworkCommon.h"
#include "NetworkMessage.h"
#include "NetworkQueue.h"
#include "NetworkConnection.h"

namespace TS
{
	namespace net
	{
		template <typename A>
		class IServer {
		public:
			IServer(uint16_t port) : m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {

			}

			virtual ~IServer() {
				stop();
			}

			bool start() {
				try {
					waitForConnection();
					m_contextThread = std::thread([this]() {
						m_asioContext.run();
					});
				}
				catch (std::exception& e) {
					std::cerr << "[Server] - Exception: " << e.what() << std::endl;
					return false;
				}

				std::cout << "[Server] - Started and waiting for Connection..." << std::endl;

				return true;
			}

			void stop() {
				m_asioContext.stop();
				m_contextThread.join();

				std::cout << "[Server] - Stopped." << std::endl;
			}

			// Asio
			void waitForConnection() {
				m_asioAcceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
					if (!ec) {
						std::cout << "[Server] - Accepted Connection: " << socket.remote_endpoint() << std::endl;
						std::shared_ptr<Connection<A>> connection = std::make_shared<Connection<A>>(Connection<A>::Owner::Server, m_asioContext, std::move(socket), m_incomingMessages);
						connection->setClientId(m_idCounter++);

						if (onClientConnect(connection)) {
							m_connections.emplace_back(std::move(connection));
							m_connections.back()->connectToClient();
							std::cout << "[Server] - Connection: " << m_connections.back()->getClientId() << " approved." << std::endl;
						}
						else {
							std::cout << "[Server] - Connection denied." << std::endl;
							--m_idCounter;
						}
					}
					else {
						std::cerr << "[Server] - Error accepting Connection: " << ec.message() << std::endl;
					}

					waitForConnection();
				});
			}

			void sendMessageToClient(std::shared_ptr<Connection<A>> client, const Message<A>& data) {
				if (client && client->isConnected()) {
					client->send(data);
				}
				else {
					onClientDisconnect(client);
					client.reset();
					m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), client), m_connections.end());
				}
			}

			void messageAllClients(const Message<A>& data, std::shared_ptr<Connection<A>> ignoreClient = nullptr) {
				bool brokenClients = false;

				for (auto& client : m_connections) {
					if (client && client->isConnected()) {
						if(client != ignoreClient) client->send(data);
					}
					else {
						onClientDisconnect(client);
						client.reset();
						brokenClients = true;
					}
				}

				if (brokenClients) {
					m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), nullptr), m_connections.end());
				}
			}

			// 20:20, -1 = max
			void update(std::size_t maxMessages = -1, bool sleep = false) {

				if (sleep) m_incomingMessages.waitForMessage();

				std::size_t messages = 0;
				while (messages < maxMessages && !m_incomingMessages.empty()) {
					auto msg = m_incomingMessages.pop_front();

					onMessage(msg.remote, msg.msg);

					++messages;
				}
			}

		protected:
			virtual bool onClientConnect(std::shared_ptr<Connection<A>> client) {
				return false;
			}

			virtual void onClientDisconnect(std::shared_ptr<Connection<A>> client) {

			}

			virtual void onMessage(std::shared_ptr<Connection<A>> client, Message<A>& data) {

			}

		protected:
			Queue<OwnedMessage<A>> m_incomingMessages;
			std::deque<std::shared_ptr<Connection<A>>> m_connections;
			asio::io_context m_asioContext;
			std::thread m_contextThread;
			asio::ip::tcp::acceptor m_asioAcceptor;
			uint32_t m_idCounter = 0;
		};
	}
}