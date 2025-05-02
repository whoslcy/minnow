#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>

class Timer
{
public:
  void activate() { active_ = true; }

  void deactivate() { active_ = false; }

  void reset() { elapsed_milliseconds_ = 0; }

  // An inactive timer doesn't tick.
  void tick( uint64_t milliseconds )
  {
    if ( active_ ) {
      elapsed_milliseconds_ += milliseconds;
    }
  }

  bool expired( uint64_t timeout ) const { return timeout <= elapsed_milliseconds_; }

private:
  uint64_t elapsed_milliseconds_ { 0 };
  bool active_ { false };
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  // Absolute Sequence Number
  using ASN = uint64_t;

  ASN FirstUnsent() const { return SYN_sent_ + reader().bytes_popped() + FIN_sent_; }

  ASN FirstAcceptable( const std::optional<TCPReceiverMessage> message ) const
  {
    // If `ackno` is std::nullopt, it means the receiver hasn't received `SYN`.
    return message && message->ackno ? message->ackno->unwrap( isn_, FirstUnsent() ) : 0;
  }

  ASN UpToDateWindowSize() const
  {
    // According to 李辰宇's implementation of TCPReceiver::send(),
    // the window size doesn't include the SYN and FIN bytes.
    return up_to_date_receiver_message_ ? up_to_date_receiver_message_->window_size : 1;
  }

  // These 3 methods are only used in the `push` method.
  void Send( const TransmitFunction& transmit, const TCPSenderMessage& message );
  ASN SenderMessageAcceptableLength() const;
  TCPSenderMessage CreateSenderMessageAsLongAsPossible( const bool SYN );

  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  bool SYN_sent_ { false };
  bool FIN_sent_ { false };

  // Below are data members for retransmission.

  std::optional<TCPReceiverMessage> up_to_date_receiver_message_ { std::nullopt };

  std::deque<TCPSenderMessage> unacknowledged_ {};
  Timer timer_ {};
  uint64_t consecutive_retransmission_count_ { 0 };
};
