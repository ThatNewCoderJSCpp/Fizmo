#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "socket_event.hpp"

namespace fizmo {
namespace networking {
namespace core {

auto SocketEventHandler::clear_all_callbacks() -> void {
        std::lock_guard<std::mutex> lock(m_handler_mutex);
        m_raw_handler.clear_all_callbacks();
        m_buffered_handler.clear_all_callbacks();
        m_buffered_udp_handler.clear_all_callbacks();
    }

} // namespace core
} // namespace networking
} // namespace fizmo
