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
		class IClient {
		public:
			IClient() : m_socket(m_context) {

			}

			virtual ~IClient() {
				disconnect();
			}

			bool connect(const std::string& host, const uint16_t port) {
				try {
					asio::ip::tcp::resolver resolver(m_context);
					auto endpoint = resolver.resolve(host, std::to_string(port));

					m_connection = std::make_unique<Connection<A>>(Connection<A>::Owner::Client, m_context, asio::ip::tcp::socket(m_context), m_incomingMessages);
					m_connection->connectToServer(endpoint);

					//m_connection->connect(endpoint);
					m_contextThread = std::thread([this]() {
						m_context.run();
					});
				}
				catch (std::exception& e) {
					std::cerr << "[Client] - Error connecting to Server: " << e.what() << std::endl;
					return false;
				}

				return true;
			}

			void disconnect() {
				if (isConnected())
					disconnect();

				m_context.stop();
				m_contextThread.join();

				m_connection.release();
			}

			bool isConnected() const {
				return m_connection ? m_connection->isConnected() : false;
			}

			void send(const Message<A>& data) {
				if(m_connection) m_connection->send(data);
			}

			Queue<OwnedMessage<A>>& getIncomingMessageQ() {
				return m_incomingMessages;
			}

		protected:
			asio::io_context m_context;
			std::thread m_contextThread;
			asio::ip::tcp::socket m_socket;
			
			std::unique_ptr<Connection<A>> m_connection;

		private:
			Queue<OwnedMessage<A>> m_incomingMessages;
		};
	}
}