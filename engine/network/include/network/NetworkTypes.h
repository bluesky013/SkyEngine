//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>

namespace sky::net {

    using ChannelId        = uint8_t;
    using MessageId        = uint16_t;
    using MessageSequence  = uint32_t;

    // Delivery semantics requested for a send. A backend that does not support a mode must reject the
    // send rather than silently downgrade it.
    enum class DeliveryMode : uint8_t {
        ReliableOrdered = 0,
        ReliableUnordered,
        UnreliableSequenced,
        Unreliable
    };

    enum class DisconnectReason : uint8_t {
        Unknown = 0,
        LocalClose,
        RemoteClose,
        Timeout,
        ProtocolError,
        Backpressure,
        ServerShutdown,
        Redirect
    };

    enum class NetResult : uint8_t {
        Ok = 0,
        NotConnected,
        InvalidHandle,
        UnsupportedDeliveryMode,
        PayloadTooLarge,
        WouldBlock,
        QueueFull,
        NotFound,
        AlreadyExists,
        InvalidArgument,
        NotAccepting,
        Closed
    };

    constexpr const char *ToString(DeliveryMode mode)
    {
        switch (mode) {
        case DeliveryMode::ReliableOrdered:    return "ReliableOrdered";
        case DeliveryMode::ReliableUnordered:  return "ReliableUnordered";
        case DeliveryMode::UnreliableSequenced:return "UnreliableSequenced";
        case DeliveryMode::Unreliable:         return "Unreliable";
        }
        return "Unknown";
    }

    constexpr const char *ToString(DisconnectReason reason)
    {
        switch (reason) {
        case DisconnectReason::Unknown:        return "Unknown";
        case DisconnectReason::LocalClose:     return "LocalClose";
        case DisconnectReason::RemoteClose:    return "RemoteClose";
        case DisconnectReason::Timeout:        return "Timeout";
        case DisconnectReason::ProtocolError:  return "ProtocolError";
        case DisconnectReason::Backpressure:   return "Backpressure";
        case DisconnectReason::ServerShutdown: return "ServerShutdown";
        case DisconnectReason::Redirect:       return "Redirect";
        }
        return "Unknown";
    }

    constexpr const char *ToString(NetResult result)
    {
        switch (result) {
        case NetResult::Ok:                      return "Ok";
        case NetResult::NotConnected:            return "NotConnected";
        case NetResult::InvalidHandle:           return "InvalidHandle";
        case NetResult::UnsupportedDeliveryMode: return "UnsupportedDeliveryMode";
        case NetResult::PayloadTooLarge:         return "PayloadTooLarge";
        case NetResult::WouldBlock:              return "WouldBlock";
        case NetResult::QueueFull:               return "QueueFull";
        case NetResult::NotFound:                return "NotFound";
        case NetResult::AlreadyExists:           return "AlreadyExists";
        case NetResult::InvalidArgument:         return "InvalidArgument";
        case NetResult::NotAccepting:            return "NotAccepting";
        case NetResult::Closed:                  return "Closed";
        }
        return "Unknown";
    }

} // namespace sky::net
