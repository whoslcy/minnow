#include "tcp_sender.hh"
#include <cassert>
#include <cstdint>
#include <optional>

#include "byte_stream.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  ASN outstanding_bytes_count { 0 };
  std::for_each( unacknowledged_.cbegin(),
                 unacknowledged_.cend(),
                 [&outstanding_bytes_count]( const TCPSenderMessage& message ) {
                   outstanding_bytes_count += message.sequence_length();
                 } );
  return outstanding_bytes_count;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmission_count_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( !SYN_sent_ ) {
    const TCPSenderMessage message { CreateSenderMessageAsLongAsPossible( true ) };
    Send( transmit, message );
    SYN_sent_ = true;
  }

  while ( true ) {
    if ( FIN_sent_ || SenderMessageAcceptableLength() == 0 ) {
      break;
    }

    const TCPSenderMessage message { CreateSenderMessageAsLongAsPossible( false ) };

    if ( !message.RST && message.sequence_length() == 0 ) {
      break;
    }

    Send( transmit, message );

    if ( message.RST ) {
      break;
    }
  }
}

void TCPSender::Send( const TransmitFunction& transmit, const TCPSenderMessage& message )
{
  transmit( message );
  if ( message.FIN ) {
    FIN_sent_ = true;
  }
  if ( message.sequence_length() ) {
    unacknowledged_.push_back( message );
    timer_.activate();
  }
}

TCPSender::ASN TCPSender::SenderMessageAcceptableLength() const
{
  return FirstAcceptable( up_to_date_receiver_message_ ) + std::max( UpToDateWindowSize(), 1UL ) - FirstUnsent();
}

TCPSenderMessage TCPSender::CreateSenderMessageAsLongAsPossible( const bool SYN )
{
  // 1. Determine the `seqno`.
  const Wrap32 seqno { Wrap32::wrap( FirstUnsent(), isn_ ) };

  // 2. Determine the payload.
  Reader& byte_stream_reader { reader() };
  const ASN acceptable_length { SenderMessageAcceptableLength() };
  const ASN acceptable_payload_size( std::min( acceptable_length - SYN, TCPConfig::MAX_PAYLOAD_SIZE ) );
  const std::string payload { byte_stream_reader.peek().substr( 0, acceptable_payload_size ) };
  byte_stream_reader.pop( payload.size() );

  // 3. Determine the FIN flag.
  const bool FIN { byte_stream_reader.is_finished() && ( SYN + payload.size() ) < acceptable_length };

  return { seqno, SYN, payload, FIN, byte_stream_reader.has_error() };
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { Wrap32::wrap( FirstUnsent(), isn_ ), false, "", false, reader().has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    reader().set_error();
    return;
  }

  ASN old_first_acceptable { FirstAcceptable( up_to_date_receiver_message_ ) };
  ASN new_first_acceptable { FirstAcceptable( std::make_optional( msg ) ) };

  const bool msg_invalid { new_first_acceptable < old_first_acceptable || FirstUnsent() < new_first_acceptable };
  if ( msg_invalid ) {
    return;
  }

  up_to_date_receiver_message_ = msg;

  const bool receiver_has_not_got_new_data { new_first_acceptable == old_first_acceptable };
  if ( receiver_has_not_got_new_data ) {
    return;
  }

  // The receiver has got new data.

  timer_.reset();
  consecutive_retransmission_count_ = 0;

  assert( !unacknowledged_.empty() );
  while ( true ) {
    const TCPSenderMessage& earliest { unacknowledged_.front() };
    const ASN after_final_byte_of_earliest_sender_message { earliest.seqno.unwrap( isn_, old_first_acceptable )
                                                            + earliest.sequence_length() };
    const bool earliest_segment_not_acknowledged { new_first_acceptable
                                                   < after_final_byte_of_earliest_sender_message };
    if ( earliest_segment_not_acknowledged ) {
      break;
    }

    // The earliest segment has been acknowledged.
    unacknowledged_.pop_front();

    const bool all_sender_messages_are_acknowledged { unacknowledged_.empty() };
    if ( all_sender_messages_are_acknowledged ) {
      timer_.deactivate();
      break;
    }
  };
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  timer_.tick( ms_since_last_tick );

  // Exponential backoff.
  const uint64_t timeout { initial_RTO_ms_ * ( 1 << consecutive_retransmission_count_ ) };

  if ( timer_.expired( timeout ) ) {
    assert( !unacknowledged_.empty() );
    transmit( unacknowledged_.front() );
    timer_.activate();
    timer_.reset();

    if ( UpToDateWindowSize() ) {
      ++consecutive_retransmission_count_;
    }
  }
}
