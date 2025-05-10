#include "network_interface.hh"

#include <iostream>
#include <optional>
#include <vector>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::SendFrame( const Serializable& data,
                                  const EthernetAddress target,
                                  const uint16_t frame_type ) const
{
  Serializer serializer {};
  data.serialize( serializer );

  const EthernetHeader header { target, ethernet_address_, frame_type };

  const EthernetFrame frame { header, serializer.finish() };
  transmit( frame );
}

void NetworkInterface::SendArpReply( const EthernetAddress target_ethernet_address,
                                     const IPv4Numeric target_ipv4_numeric ) const
{
  ARPMessage arp_reply;
  arp_reply.opcode = ARPMessage::OPCODE_REPLY;
  arp_reply.sender_ethernet_address = ethernet_address_;
  arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
  arp_reply.target_ethernet_address = target_ethernet_address;
  arp_reply.target_ip_address = target_ipv4_numeric;
  SendFrame( arp_reply, target_ethernet_address, EthernetHeader::TYPE_ARP );
}

void NetworkInterface::BroadcastArpRequest( const IPv4Numeric unknown_ipv4_numeric ) const
{
  ARPMessage arp_request;
  arp_request.opcode = ARPMessage::OPCODE_REQUEST;
  arp_request.sender_ethernet_address = ethernet_address_;
  arp_request.sender_ip_address = ip_address_.ipv4_numeric();
  arp_request.target_ip_address = unknown_ipv4_numeric;
  SendFrame( arp_request, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP );
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_hop_ipv4_numeric { next_hop.ipv4_numeric() };

  const std::optional<EthernetAddress> ethernet_address { arp_table_.Query( next_hop_ipv4_numeric ) };
  if ( ethernet_address ) {
    SendFrame( dgram, *ethernet_address, EthernetHeader::TYPE_IPv4 );
    return;
  }

  const bool no_arp_request_was_sent_for_the_same_IP_address_in_the_last_5_seconds {
    !sent_arp_requests_.contains( next_hop_ipv4_numeric ) };
  if ( no_arp_request_was_sent_for_the_same_IP_address_in_the_last_5_seconds ) {
    BroadcastArpRequest( next_hop_ipv4_numeric );
    sent_arp_requests_.insert( { next_hop_ipv4_numeric, 0 } );
  }

  unroutable_datagrams_[next_hop_ipv4_numeric].emplace_back( dgram );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  const bool not_destined_for_this_interface { frame.header.dst != ethernet_address_
                                               && frame.header.dst != ETHERNET_BROADCAST };
  if ( not_destined_for_this_interface ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    Parser parser { frame.payload };
    InternetDatagram dgram;
    dgram.parse( parser );
    if ( !parser.has_error() ) {
      datagrams_received_.push( dgram );
    }
    return;
  }

  // frame.header.type == EthernetHeader::TYPE_ARP

  Parser parser { frame.payload };
  ARPMessage arp_message;
  arp_message.parse( parser );
  if ( parser.has_error() ) {
    return;
  }

  arp_table_.Add( arp_message.sender_ip_address, arp_message.sender_ethernet_address );

  const auto search { unroutable_datagrams_.find( arp_message.sender_ip_address ) };
  if ( search != unroutable_datagrams_.end() ) {
    const auto& datagrams { search->second };
    for ( const auto& datagram : datagrams ) {
      SendFrame( datagram.datagram, arp_message.sender_ethernet_address, EthernetHeader::TYPE_IPv4 );
    }
    unroutable_datagrams_.erase( search );
  }

  if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST
       && arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
    SendArpReply( arp_message.sender_ethernet_address, arp_message.sender_ip_address );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  constexpr size_t kArpRequestInterval { 5000 };
  std::erase_if( unroutable_datagrams_, [kArpRequestInterval, ms_since_last_tick]( auto& pair ) {
    auto& datagrams { pair.second };

    // Discard datagrams when associated ARP request expires.
    while ( true ) {
      if ( datagrams.empty() ) {
        break;
      }
      auto& earliest_datagram = datagrams.front();

      earliest_datagram.arp_request_timer += ms_since_last_tick;
      if ( earliest_datagram.arp_request_timer < kArpRequestInterval ) {
        break;
      }

      datagrams.pop_front();
    }

    // Update ARP request timers of pending datagrams.
    for ( auto& datagram : datagrams ) {
      datagram.arp_request_timer += ms_since_last_tick;
    }

    return datagrams.empty();
  } );

  std::erase_if( sent_arp_requests_, [kArpRequestInterval, ms_since_last_tick]( auto& pair ) {
    auto& timer { pair.second };
    timer += ms_since_last_tick;
    return kArpRequestInterval < timer;
  } );

  arp_table_.Tick( ms_since_last_tick );
}
