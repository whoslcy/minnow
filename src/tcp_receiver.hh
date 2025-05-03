#pragma once

#include "byte_stream.hh"
#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPReceiver
{
public:
  // Construct with given Reassembler
  explicit TCPReceiver( Reassembler&& reassembler ) : reassembler_( std::move( reassembler ) ) {}

  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into
   * the Reassembler at the correct stream index.
   */
  void receive( TCPSenderMessage message );

  // The TCPReceiver sends TCPReceiverMessages to the peer's TCPSender.
  TCPReceiverMessage send() const;

  // Access the output
  const Reassembler& reassembler() const { return reassembler_; }
  Reader& reader() { return reassembler_.reader(); }
  const Reader& reader() const { return reassembler_.reader(); }
  const Writer& writer() const { return reassembler_.writer(); }

private:
  uint64_t AbsoluteSequenceNumberOfFirstUnassembled() const { return reassembler_.writer().bytes_pushed() + 1; }

  bool IsSynReceived() const { return zero_point_.has_value(); }

  bool is_finished_ { false };
  std::optional<Wrap32> zero_point_ { std::nullopt };
  Reassembler reassembler_;
};
