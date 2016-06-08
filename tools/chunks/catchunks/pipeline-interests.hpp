/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2016,  Regents of the University of California,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University.
 *
 * This file is part of ndn-tools (Named Data Networking Essential Tools).
 * See AUTHORS.md for complete list of ndn-tools authors and contributors.
 *
 * ndn-tools is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndn-tools is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndn-tools, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 *
 * @author Wentao Shang
 * @author Steve DiBenedetto
 * @author Andrea Tosatto
 */

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_HPP

#include "core/common.hpp"
#include "options.hpp"
#include <queue>
#include "rtt-estimator.hpp"

namespace ndn {
namespace chunks {

class DataFetcher;

class PipelineInterestsOptions : public Options
{
public:
  explicit
  PipelineInterestsOptions(const Options& options = Options())
    : Options(options)
    , maxPipelineSize(0)
    , startPipelineSize(1)
    , slowStartThreshold(20)
    , nTimeoutBeforeReset(3)
    , windowCutMultiplier(0.75)
    , rtoMultiplierReset(false)
  {
  }

public:
  size_t maxPipelineSize;
  size_t startPipelineSize;
  size_t slowStartThreshold;
  size_t nTimeoutBeforeReset;
  float windowCutMultiplier;
  bool rtoMultiplierReset;
};

/**
 * @brief Service for retrieving Data via an Interest pipeline
 *
 * Retrieves all segmented Data under the specified prefix by maintaining a pipeline of N Interests
 * in flight.
 *
 * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
 * PipelineInterests' user via callback immediately upon arrival.
 */
class PipelineInterests
{
public:
  typedef PipelineInterestsOptions Options;

  typedef function<void(const std::string& reason)> FailureCallback;

  typedef function<void(const Interest&, const Data&, const shared_ptr<DataFetcher>&)> DataFetcherDoneCallback;

public:
  /**
   * @brief create a PipelineInterests service
   *
   * Configures the pipelining service without specifying the retrieval namespace. After this
   * configuration the method runWithExcludedSegment must be called to run the Pipeline.
   */
  explicit
  PipelineInterests(Face& face, const Options& options = Options(), uint64_t randomWaitMax = 0, bool startWait = false);

  ~PipelineInterests();

  /**
   * @brief fetch all the segments between 0 and lastSegment of the specified prefix
   *
   * Starts the pipeline of size defined inside the options. The pipeline retrieves all the segments
   * until the last segment is received, @p data is excluded from the retrieving.
   *
   * @param data a segment of the segmented Data to retrive; data.getName() must end with a segment
   *        number
   * @param onData callback for every segment correctly received, must not be empty
   * @param onfailure callback called if an error occurs
   */
  void
  runWithExcludedSegment(const Data& data, DataCallback onData, FailureCallback onFailure);

  void
  runWithName(Name nameWithVersion, DataCallback onData, FailureCallback onFailure);

  /**
   * @brief stop all fetch operations
   */
  void
  cancel();

  bool
  setWindowSize(float size);

  float
  getWindowSize() const;

  time::milliseconds
  getInterestLifetime();

private:
  /**
   * @brief fetch the next segment that has not been requested yet
   *
   * @return false if there is an error or all the segments have been fetched, true otherwise
   */
  bool
  fetchNextSegment(size_t pipeNo);

  void
  deferredFetchNextSegment(size_t pipeNo);

  void
  fail(const std::string& reason);

  void
  handleData(const Interest& interest, const Data& data, const shared_ptr<DataFetcher>& dataFetcher, size_t pipeNo);

  void
  handleError(const std::string& reason, size_t pipeNo);

  void
  handleFail(const std::string& reason, size_t pipeNo);

  bool
  canSend(uint64_t segmentNo, uint64_t pipeNo);

  void
  handleWindowEvent();

private:
  Name m_prefix;
  Face& m_face;
  uint64_t m_nextSegmentNo;
  uint64_t m_lastSegmentNo;
  uint64_t m_excludeSegmentNo;
  DataCallback m_onData;
  FailureCallback m_onFailure;
  const Options m_options;
  std::vector<std::pair<shared_ptr<DataFetcher>, uint64_t>> m_segmentFetchers;
  bool m_hasFinalBlockId;
  /**
   * true if there's a critical error
   */
  bool m_hasError;
  /**
   * true if one or more segmentFetcher failed, if lastSegmentNo is not set this is usually not a
   * fatal error for the pipeline
   */
  bool m_hasFailure;

  uint64_t m_randomWaitMax;
  Scheduler m_scheduler;
  std::mt19937 m_randomGen;
  bool m_startWait;

  // Congestion control
  float m_currentWindowSize;
  float m_calculatedWindowSize;
  float m_lastWindowSize;
  std::queue<uint64_t/*Pipe number*/>  m_waitingPipes;
  std::queue<uint64_t/*Segment number*/>  m_waitingSegments;

  uint64_t m_nMissingWindowEvents; // TODO better name
  bool m_isWindowCut; // TODO better name
  float m_windowCutMultiplier;
  bool m_hasMultiplierChanged; // for this window

  size_t m_nConsecutiveTimeouts;

public:
  RttEstimator rttEstimator;
};

} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_HPP
