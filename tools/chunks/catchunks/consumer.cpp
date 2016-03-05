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

Consumer::Consumer(Face& face, Validator& validator, bool isVerbose, std::ostream& os)
  : m_face(face)
  , m_validator(validator)
  , m_pipeline(nullptr)
  , m_nextToPrint(0)
  , m_outputStream(os)
  , m_isVerbose(isVerbose)
  , m_receivedBytes(0)
{
}

void Consumer::run(DiscoverVersion& discover, PipelineInterests& pipeline)
{
  m_pipeline = &pipeline;
  m_nextToPrint = 0;

  discover.onDiscoverySuccess.connect(bind(&Consumer::runWithData, this, _1));
  discover.onDiscoveryFailure.connect(bind(&Consumer::onFailure, this, _1));

  discover.run();
  m_face.processEvents();
}

void Consumer::runWithData(const Data& data)
{
  m_validator.validate(data,
                       bind(&Consumer::onDataValidated, this, _1),
                       bind(&Consumer::onFailure, this, _2));

  m_pipeline->runWithExcludedSegment(data,
                                     bind(&Consumer::onData, this, _1, _2),
                                     bind(&Consumer::onFailure, this, _1));

  m_receivedBytes = 0;
  m_startTime = time::steady_clock::now();
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

  m_bufferedData[data->getName()[-1].toSegment()] = data;

  m_receivedBytes += data->getContent().value_size(); //TODO
  time::milliseconds runningTime = time::duration_cast<time::milliseconds> (time::steady_clock::now() - m_startTime);
  //runningTime += time::seconds(1);

  //tracelog(TRACE_INFO, "my message, my integer: %d", 2);
  //std::cerr << data->getName()[-1].toSegment() << "Size: " << data->getContent().value_size() << " time: " << runningTime.count() << std::endl;

  if (data->getName()[-1].toSegment() == data->getFinalBlockId().toSegment()) { //TODO errore final block id
   std::cerr << "Received: " << m_receivedBytes << " bytes - Time: " << (runningTime.count() / 1000) << " seconds" <<std::endl;
   std::cerr << "Speed: " << static_cast<double>(m_receivedBytes/1000) / (runningTime.count() / 1000) << " KB/s" <<std::endl;
  }

  writeInOrderData();
}

void
Consumer::onFailure(const std::string& reason)
{
  throw std::runtime_error(reason);
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
