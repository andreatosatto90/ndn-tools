#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER chunksLog

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "tools/chunks/chunks-tracepoint.hpp"

#if !defined(NDN_TOOLS_CHUNKS_CHUNKS_TRACEPOINT_HPP) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define NDN_TOOLS_CHUNKS_CHUNKS_TRACEPOINT_HPP

#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(
  chunksLog,
  cat_started,
  TP_ARGS(
    int, maxPipelineSize,
    int, interestLifetime,
    int, maxRetries,
    int, mustBeFresh
  ),
  TP_FIELDS(
    ctf_integer(int, max_pipeline_size, maxPipelineSize)
    ctf_integer(int, interest_lifetime, interestLifetime)
    ctf_integer(int, max_retries, maxRetries)
    ctf_integer(int, must_be_fresh, mustBeFresh)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  cat_stopped,
  TP_ARGS(
    int, exitCode
  ),
  TP_FIELDS(
    ctf_integer(int, exit_code, exitCode)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  put_started,
  TP_ARGS(
    const char*, prefix,
    const char*, signingInfo,
    int, freshness,
    int, maxSegmentSize,
    int, numberOfSegments
  ),
  TP_FIELDS(
    ctf_string(prefix, prefix)
    ctf_string(signing_info, signingInfo)
    ctf_integer(int, freshness, freshness)
    ctf_integer(int, max_segment_size, maxSegmentSize)
    ctf_integer(int, number_of_segments, numberOfSegments)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  data_discovery,
  TP_ARGS(
    int, segmentNo,
    int, bytes
  ),
  TP_FIELDS(
    ctf_integer(int, bytes, bytes)
    ctf_integer(int, segment_number, segmentNo)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  data_received,
  TP_ARGS(
    int, segmentNo,
    int, bytes
  ),
  TP_FIELDS(
    ctf_integer(int, bytes, bytes)
    ctf_integer(int, segment_number, segmentNo)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  data_sent,
  TP_ARGS(
    int, segmentNo,
    int, bytes
  ),
  TP_FIELDS(
    ctf_integer(int, bytes, bytes)
    ctf_integer(int, segment_number, segmentNo)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  interest_discovery,
  TP_ARGS(              // TODO no ARGS
    int, segmentNo
  ),
  TP_FIELDS(
    ctf_integer(int, segment_number, segmentNo)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  interest_sent,
  TP_ARGS(
    int, segmentNo
  ),
  TP_FIELDS(
    ctf_integer(int, segment_number, segmentNo)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  interest_timeout,
  TP_ARGS(
    int, segmentNo
  ),
  TP_FIELDS(
    ctf_integer(int, segment_number, segmentNo)
  )
)

TRACEPOINT_EVENT(
  chunksLog,
  interest_nack,
  TP_ARGS(
    int, segmentNo
  ),
  TP_FIELDS(
    ctf_integer(int, segment_number, segmentNo)
  )
)

#endif // NDN_TOOLS_CHUNKS_CHUNKS_TRACEPOINT_HPP

#include <lttng/tracepoint-event.h>
