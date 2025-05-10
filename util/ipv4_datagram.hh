#pragma once

#include <string>
#include <vector>

#include "ipv4_header.hh"
#include "parser.hh"
#include "ref.hh"
#include "serializable.hh"

//! \brief [IPv4](\ref rfc::rfc791) Internet datagram
struct IPv4Datagram : Serializable
{
  IPv4Header header {};
  std::vector<Ref<std::string>> payload {};

  IPv4Datagram() = default;

  IPv4Datagram( const IPv4Header& ipv4_header, std::vector<Ref<std::string>> datagram_payload )
    : header { ipv4_header }, payload { std::move( datagram_payload ) }
  {}

  void parse( Parser& parser )
  {
    header.parse( parser );
    parser.truncate( header.payload_length() );
    parser.all_remaining( payload );
  }

  void serialize( Serializer& serializer ) const
  {
    header.serialize( serializer );
    serializer.buffer( payload );
  }
};

using InternetDatagram = IPv4Datagram;
