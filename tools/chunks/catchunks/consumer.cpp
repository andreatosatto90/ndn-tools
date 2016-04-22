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

#include "consumer.hpp"
#include "discover-version.hpp"

namespace ndn {
namespace chunks {

Consumer::Consumer(Face& face, Validator& validator, bool isVerbose, std::ostream& os,
                   bool printStat, int pipelineIncrease)
  : m_face(face)
  , m_validator(validator)
  , m_pipeline(nullptr)
  , m_nextToPrint(0)
  , m_outputStream(os)
  , m_isVerbose(isVerbose)
  , m_printStat(printStat)
  , m_scheduler(face.getIoService())
  , m_windowMultiplier(pipelineIncrease)
{
  m_statIntervalMs = 500;
}

void
Consumer::run(DiscoverVersion& discover, PipelineInterests& pipeline, bool noDiscovery)
{
  m_pipeline = &pipeline;
  m_nextToPrint = 0;

  if (!noDiscovery) {
    discover.onDiscoverySuccess.connect(bind(&Consumer::runWithData, this, _1));
    discover.onDiscoveryFailure.connect(bind(&Consumer::onFailure, this, _1));
    discover.run();
  }
  else{
    runWithName(discover.m_prefix);
  }


  m_face.processEvents();
}

void
Consumer::cancel()
{
  m_face.getIoService().stop();
}

void
Consumer::runWithData(const Data& data)
{
  m_nReceivedSegments = 0;
  m_lastSegmentNo = 0;
  m_receivedBytes = 0;
  m_lastReceivedBytes = 0;

  m_validator.validate(data,
                       bind(&Consumer::onDataValidated, this, _1),
                       bind(&Consumer::onFailure, this, _2));

  m_pipeline->runWithExcludedSegment(data,
                                     bind(&Consumer::onData, this, _1, _2),
                                     bind(&Consumer::onFailure, this, _1));

  m_startTime = time::steady_clock::now();
  m_lastPrintTime = m_startTime;

  if (m_printStat) {
    m_scheduler.scheduleEvent(time::milliseconds(m_statIntervalMs), bind(&Consumer::printStatistics, this));
  }
}

void
Consumer::runWithName(Name nameWithVersion)
{
  m_nReceivedSegments = 0;
  m_lastSegmentNo = std::numeric_limits<uint64_t>::max();
  m_receivedBytes = 0;
  m_lastReceivedBytes = 0;


  m_pipeline->runWithName(nameWithVersion,
                          bind(&Consumer::onData, this, _1, _2),
                          bind(&Consumer::onFailure, this, _1));

  m_startTime = time::steady_clock::now();
  m_lastPrintTime = m_startTime;

  if (m_printStat) {
    m_scheduler.scheduleEvent(time::seconds(1), bind(&Consumer::printStatistics, this));
  }
}

void
Consumer::onData(const Interest& interest, const Data& data)
{
  m_validator.validate(data,
                       bind(&Consumer::onDataValidated, this, _1),
                       bind(&Consumer::onFailure, this, _2));
}

void
Consumer::onDataValidated(shared_ptr<const Data> data)
{
  if (data->getContentType() == ndn::tlv::ContentType_Nack) {
    if (m_isVerbose)
      std::cerr << "Application level NACK: " << *data << std::endl;

    m_pipeline->cancel();
    throw ApplicationNackError(*data);
  }

  m_lastSegmentNo = data->getFinalBlockId().toSegment();

  m_bufferedData[data->getName()[-1].toSegment()] = data;

  m_receivedBytes += data->getContent().value_size();
  m_lastReceivedBytes += data->getContent().value_size();

  m_nReceivedSegments++;

  writeInOrderData();
}

void
Consumer::onFailure(const std::string& reason)
{
  throw std::runtime_error(reason);
}

void
Consumer::printStatistics()
{
  time::milliseconds runningTime =
      time::duration_cast<time::milliseconds> (time::steady_clock::now() - m_startTime);
  time::milliseconds lastRunningTime =
      time::duration_cast<time::milliseconds> (time::steady_clock::now() - m_lastPrintTime);

  if (m_receivedBytes > 0) {
    int perc = (static_cast<float>(m_nReceivedSegments) / m_lastSegmentNo) * 100;
    std::cerr << perc << "% \t"
              << " T " << static_cast<double>(m_receivedBytes/1000) << " KB \t"
              << static_cast<double>(m_receivedBytes) / (runningTime.count()) << " KB/s \t"
              << " C " << static_cast<double>(m_lastReceivedBytes/1000) << " KB  \t"
              << static_cast<double>(m_lastReceivedBytes) / (lastRunningTime.count()) << " KB/s \t"
              << "Window " << m_pipeline->getWindowSize() << "\t"
              << "Rtt " << int(m_pipeline->rttEstimator.getRttMean()) << "\t"
              << std::endl;
  }
  else {
    std::cerr << "Waiting first data" << std::endl;
  }

  m_lastPrintTime = time::steady_clock::now();
  m_lastReceivedBytes = 0;

  /*if(!m_pipeline->setWindowSize(m_pipeline->getWindowSize() + m_windowMultiplier)) {
    m_windowMultiplier *= -1;
    m_pipeline->setWindowSize(m_pipeline->getWindowSize() + m_windowMultiplier);
  }*/

  if (m_nReceivedSegments < m_lastSegmentNo) {
    m_scheduler.scheduleEvent(time::milliseconds(m_statIntervalMs), bind(&Consumer::printStatistics, this));
  }
  else
    m_face.getIoService().stop();
}

void
Consumer::writeInOrderData()
{
  for (auto it = m_bufferedData.begin();
       it != m_bufferedData.end() && it->first == m_nextToPrint;
       it = m_bufferedData.erase(it), ++m_nextToPrint) {

    const Block& content = it->second->getContent();
    m_outputStream.write(reinterpret_cast<const char*>(content.value()), content.value_size());
  }
}

} // namespace chunks
} // namespace ndn
