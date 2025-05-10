#include "tcp_receiver.hh"

#include <cstdint>
#include <limits>
#include <optional>

#include "byte_stream.hh"
#include "debug.hh"
#include "wrapping_integers.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }

  if ( message.SYN ) {
    is_finished_ = false;
    zero_point_ = message.seqno;
  }

  // Indecies of the first byte of the payload.
  const Wrap32 sequence_number { message.SYN ? message.seqno + 1 : message.seqno };
  const uint64_t absolute_sequence_number {
    sequence_number.unwrap( *zero_point_, AbsoluteSequenceNumberOfFirstUnassembled() ) };
  const uint64_t byte_stream_index { absolute_sequence_number - 1 };

  reassembler_.insert( byte_stream_index, message.payload, message.FIN );

  if ( reassembler_.writer().is_closed() ) {
    is_finished_ = true;
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  const uint64_t absolute_sequence_number { AbsoluteSequenceNumberOfFirstUnassembled() + is_finished_ };
  const std::optional<Wrap32> ackno
    = zero_point_ ? std::make_optional( Wrap32::wrap( absolute_sequence_number, *zero_point_ ) ) : std::nullopt;
  const uint16_t window_size = std::min( reassembler_.writer().available_capacity(),
                                         static_cast<uint64_t>( std::numeric_limits<uint16_t>::max() ) );
  const bool RST = reassembler_.writer().has_error();
  return { ackno, window_size, RST };
}
