#pragma once

#include "NetworkCommon.h"

namespace TS
{
	namespace net
	{
		template <typename A>
		struct MessageHeader {
			A id {};
			uint32_t size = 0;
		};

		template <typename A>
		struct Message {
			MessageHeader<A> header {};
			std::vector<uint8_t> body;

			std::size_t size() const {
				return /*sizeof(MessageHeader<A>) + */ body.size();
			}

			// Cout
			 std::ostream& operator << (std::ostream& os) {
				os << "ID: " << header.id << ", Size: " << header.size;
				return os;
			}

			// Push data into body
			template <typename DataType>
			Message<A>& operator<<(const DataType data) {
				static_assert(std::is_standard_layout<DataType>::value, "Only basic data types can be pushed into a Message.");
				
				std::size_t currentSize = body.size();
				body.resize(body.size() + sizeof(DataType));

				// Copy data
				std::memcpy(body.data() + currentSize, &data, sizeof(DataType));
				header.size = (uint32_t)size();

				return *this;
			}

			// Pop data out of body
			template <typename DataType>
			Message<A>& operator>>(DataType& data) {
				static_assert(std::is_standard_layout<DataType>::value, "Can only pop data from Messages into basic data types.");
				if (body.size() == 0) {
					std::cerr << "[TSMessage] Critical: Cannot extract data from a empty message!" << std::endl;
					return *this;
				}

				std::memcpy(&data, body.data() + (body.size() - sizeof(DataType)), sizeof(DataType));

				// Maybe better with resize?
				body.erase(body.end() - sizeof(DataType), body.end());
				header.size = (uint32_t)size();

				return *this;
			}
		};

		template <typename A>
		class Connection;

		template <typename A>
		struct OwnedMessage {
			std::shared_ptr<Connection<A>> remote = nullptr;
			Message<A> msg;

			// Cout
			std::ostream& operator<<(std::ostream& os) {
				os << msg.body;
				return os;
			}
		};
	}
}