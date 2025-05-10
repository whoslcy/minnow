#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <queue>

#include "address.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "serializable.hh"

// Address Resolution Protocol
class ArpTable
{
  using IPv4Numeric = uint32_t;

public:
  std::optional<EthernetAddress> Query( const IPv4Numeric ipv4_numeric ) const
  {
    const auto search = arp_table_.find( ipv4_numeric );
    if ( search != arp_table_.end() ) {
      return search->second.ethernet_address;
    }
    return std::nullopt;
  }

  void Add( const IPv4Numeric ipv4_numeric, const EthernetAddress ethernet_address )
  {
    arp_table_[ipv4_numeric] = { ethernet_address, 0 };
  }

  void Tick( const size_t ms_since_last_tick )
  {
    // 30 seconds.
    constexpr size_t kAddressDuration { 30000 };

    for ( auto& [ip, entry] : arp_table_ ) {
      entry.timer += ms_since_last_tick;
    }
    std::erase_if( arp_table_, [&]( const auto& entry ) { return kAddressDuration < entry.second.timer; } );
  }

private:
  struct EthernetAddressWithTimer
  {
    EthernetAddress ethernet_address;
    size_t timer;
  };

  std::map<IPv4Numeric, EthernetAddressWithTimer> arp_table_ {};
};

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( EthernetFrame frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  using IPv4Numeric = uint32_t;
  using Timer = size_t;

  struct DirectionlessDatagramWithArpRequestTimer
  {
    InternetDatagram datagram;
    size_t arp_request_timer { 0 };

    DirectionlessDatagramWithArpRequestTimer( const InternetDatagram internet_datagram )
      : datagram { internet_datagram }
    {}
  };

  void SendFrame( const Serializable& data, EthernetAddress target, uint16_t frame_type ) const;

  void SendArpReply( EthernetAddress target_ethernet_address, IPv4Numeric target_ipv4_numeric ) const;

  void BroadcastArpRequest( uint32_t unknown_ip_address ) const;

  std::map<IPv4Numeric, std::deque<DirectionlessDatagramWithArpRequestTimer>> unroutable_datagrams_ {};

  std::map<IPv4Numeric, Timer> sent_arp_requests_ {};

  ArpTable arp_table_ {};

  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};
};
