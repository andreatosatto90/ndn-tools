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

#include "pipeline-interests.hpp"
#include "data-fetcher.hpp"

namespace ndn {
namespace chunks {

PipelineInterests::PipelineInterests(Face& face, const Options& options, uint64_t randomWaitMax, bool startWait)
  : m_face(face)
  , m_nextSegmentNo(0)
  , m_lastSegmentNo(0)
  , m_excludeSegmentNo(0)
  , m_options(options)
  , m_hasFinalBlockId(false)
  , m_hasError(false)
  , m_hasFailure(false)
  , m_randomWaitMax(randomWaitMax)
  , m_scheduler(face.getIoService())
  , m_startWait(startWait)
  , m_currentWindowSize(m_options.startPipelineSize)
  , m_calculatedWindowSize(m_options.startPipelineSize)
{
  BOOST_ASSERT(m_options.maxPipelineSize >= m_options.startPipelineSize);

  m_segmentFetchers.resize(m_options.maxPipelineSize);
  std::random_device rd;
  m_randomGen.seed(rd());
}

PipelineInterests::~PipelineInterests()
{
  cancel();
}

void
PipelineInterests::runWithExcludedSegment(const Data& data, DataCallback onData,
                                          FailureCallback onFailure)
{
  BOOST_ASSERT(onData != nullptr);
  m_onData = std::move(onData);
  m_onFailure = std::move(onFailure);

  Name dataName = data.getName();
  m_prefix = dataName.getPrefix(-1);
  m_excludeSegmentNo = dataName[-1].toSegment();

  if (!data.getFinalBlockId().empty()) {
    m_hasFinalBlockId = true;
    m_lastSegmentNo = data.getFinalBlockId().toSegment();
  }

  // if the FinalBlockId is unknown, this could potentially request non-existent segments
  for (size_t nRequestedSegments = 0; nRequestedSegments < m_options.startPipelineSize;
       nRequestedSegments++) {
    deferredFetchNextSegment(nRequestedSegments);
    //if (!fetchNextSegment(nRequestedSegments)) // all segments have been requested
      //break;
  }

  for (size_t nWaitingSegments = m_options.startPipelineSize; nWaitingSegments < m_options.maxPipelineSize;
       nWaitingSegments++) {
    m_waitingPipes.push(nWaitingSegments);
  }

  m_currentWindowSize = m_options.startPipelineSize;
}

void
PipelineInterests::runWithName(Name nameWithVersion, DataCallback onData, FailureCallback onFailure)
{
  BOOST_ASSERT(onData != nullptr);
  m_onData = std::move(onData);
  m_onFailure = std::move(onFailure);

  m_prefix = nameWithVersion;
  m_excludeSegmentNo = std::numeric_limits<uint64_t>::max();

  // if the FinalBlockId is unknown, this could potentially request non-existent segments
  for (size_t nRequestedSegments = 0; nRequestedSegments < m_options.startPipelineSize;
       nRequestedSegments++) {
    deferredFetchNextSegment(nRequestedSegments);
  }

  for (size_t nWaitingSegments = m_options.startPipelineSize; nWaitingSegments < m_options.maxPipelineSize;
       nWaitingSegments++) {
    m_waitingPipes.push(nWaitingSegments);
  }

  m_currentWindowSize = m_options.startPipelineSize;
}

bool
PipelineInterests::fetchNextSegment(std::size_t pipeNo)
{
  if (m_hasFailure) {
    fail("Fetching terminated but no final segment number has been found");
    return false;
  }


  uint64_t segmentNo = m_nextSegmentNo;

  if (m_waitingSegments.size() > 0) {
    segmentNo = m_waitingSegments.front();
    m_waitingSegments.pop();

    std::cerr << "Pipe: " << pipeNo << " Requesting segment #" << segmentNo << " next segment no " << m_nextSegmentNo << std::endl;

  }
  else{
    ++m_nextSegmentNo;
  }

  if (segmentNo == m_excludeSegmentNo) {
    ++segmentNo;
  }

  if (m_hasFinalBlockId && segmentNo > m_lastSegmentNo)
   return false;


  // Send interest for next segment
  if (m_options.isVerbose)
    std::cerr << "Pipe: " << pipeNo << " Requesting segment #" << segmentNo << std::endl;

  Interest interest(Name(m_prefix).appendSegment(segmentNo));
  interest.setInterestLifetime(m_options.interestLifetime);
  interest.setMustBeFresh(m_options.mustBeFresh);
  interest.setMaxSuffixComponents(1);

  BOOST_ASSERT(!m_segmentFetchers[pipeNo].first || !m_segmentFetchers[pipeNo].first->isRunning());

  auto fetcher = DataFetcher::fetch(m_face, interest,
                                    m_options.maxRetriesOnTimeoutOrNack,
                                    m_options.maxRetriesOnTimeoutOrNack,
                                    bind(&PipelineInterests::handleData, this, _1, _2, pipeNo),
                                    bind(&PipelineInterests::handleFail, this, _2, pipeNo),
                                    bind(&PipelineInterests::handleFail, this, _2, pipeNo),
                                    m_options.isVerbose,
                                    bind(&PipelineInterests::canSend, this, segmentNo, pipeNo));

  m_segmentFetchers[pipeNo] = make_pair(fetcher, segmentNo);

  return true;
}

void
PipelineInterests::deferredFetchNextSegment(std::size_t pipeNo)
{
  if (m_randomWaitMax !=0 ) {
    std::uniform_int_distribution<> dis(0, m_randomWaitMax);
    int randomValue = dis(m_randomGen);

    if (randomValue > 0) {
      m_scheduler.scheduleEvent(time::milliseconds(randomValue), bind(&PipelineInterests::fetchNextSegment, this, pipeNo));
      return;
    }
  }

  fetchNextSegment(pipeNo);
}

void
PipelineInterests::cancel()
{
  for (auto& fetcher : m_segmentFetchers)
    if (fetcher.first)
      fetcher.first->cancel();

  m_segmentFetchers.clear();
}

bool
PipelineInterests::setWindowSize(uint64_t size)
{
  if (size >= m_options.startPipelineSize && size <= m_options.maxPipelineSize) {
    m_calculatedWindowSize = size;
    std::cerr << "Window size: " << m_calculatedWindowSize << std::endl;
    return true;
  }

  return false;
}

uint64_t
PipelineInterests::getWindowSize() const
{
  return m_calculatedWindowSize;
}

void
PipelineInterests::fail(const std::string& reason)
{
  if (!m_hasError) {
    cancel();
    m_hasError = true;
    m_hasFailure = true;
    if (m_onFailure)
      m_face.getIoService().post([this, reason] { m_onFailure(reason); });
  }
}

void
PipelineInterests::handleData(const Interest& interest, const Data& data, size_t pipeNo)
{
  if (m_hasError)
    return;

  BOOST_ASSERT(data.getName().equals(interest.getName()));

  //std::cerr << data.getTag<lp::StrategyNotify>() << std::endl;

  if (m_options.isVerbose)
    std::cerr << "Pipe: " << pipeNo << " Received segment #" << data.getName()[-1].toSegment() << std::endl;

  m_onData(interest, data);

  if (!m_hasFinalBlockId && !data.getFinalBlockId().empty()) {
    m_lastSegmentNo = data.getFinalBlockId().toSegment();
    m_hasFinalBlockId = true;

    for (auto& fetcher : m_segmentFetchers) {
      if (fetcher.first && fetcher.second > m_lastSegmentNo) {
        // Stop trying to fetch segments that are not part of the content
        fetcher.first->cancel();
      }
      else if (fetcher.first && fetcher.first->hasError()) { // fetcher.second <= m_lastSegmentNo
        // there was an error while fetching a segment that is part of the content
        fail("Failure retriving segment #" + to_string(fetcher.second));
        return;
      }
    }
  }

  m_currentWindowSize--;
  m_waitingPipes.push(pipeNo);

  while (m_currentWindowSize < m_calculatedWindowSize) {
    if (m_startWait)
      fetchNextSegment(m_waitingPipes.front());
    else
      deferredFetchNextSegment(m_waitingPipes.front());

    m_waitingPipes.pop();

    ++m_currentWindowSize;
  }

  // TODO handle window decrease
}

void
PipelineInterests::handleFail(const std::string& reason, std::size_t pipeNo)
{
  if (m_hasError)
    return;

  if (m_hasFinalBlockId && m_segmentFetchers[pipeNo].second <= m_lastSegmentNo) {
    fail(reason);
  }
  else if (!m_hasFinalBlockId) {
    // don't fetch the following segments
    bool areAllFetchersStopped = true;
    for (auto& fetcher : m_segmentFetchers) {
      if (fetcher.first && fetcher.second > m_segmentFetchers[pipeNo].second) {
        fetcher.first->cancel();
      }
      else if (fetcher.first && fetcher.first->isRunning()) {
        // fetcher.second <= m_segmentFetchers[pipeNo].second
        areAllFetchersStopped = false;
      }
    }
    if (areAllFetchersStopped) {
      if (m_onFailure)
        fail("Fetching terminated but no final segment number has been found");
    }
    else {
      m_hasFailure = true;
    }
  }
}

bool
PipelineInterests::canSend(uint64_t segmentNo, uint64_t pipeNo)
{
  if (m_currentWindowSize <= m_calculatedWindowSize)
    return true;

  m_waitingPipes.push(pipeNo);
  m_waitingSegments.push(segmentNo);
  return false;
}

} // namespace chunks
} // namespace ndn
