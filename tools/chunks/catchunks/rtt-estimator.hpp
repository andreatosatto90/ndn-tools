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

#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_RTT_ESTIMATOR_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_RTT_ESTIMATOR_HPP

#include "core/common.hpp"


namespace ndn {
namespace chunks {

class DataFetcher;

class RttEstimator
{
public:
  RttEstimator();

  float addRttMeasurement(const shared_ptr<DataFetcher>& df);

  float getRTO() const;

  float getRttMean() const;

  float getRttVar() const;

  float incrementRtoMultiplier();

  float decrementRtoMultiplier();

  float getRtoMultiplier();

  void reset();

private :
  float m_rttMean;
  float m_rttVar;
  float m_rtt0;
  float m_rttMulti;
  float m_rttMax;
  float m_rttMin;
  float m_lastRtt;
  float m_rttMinCalc;
  float m_rtoMulti;

  std::vector<float> m_oldRtt;
  uint32_t m_nSamples;
  std::pair<float /*old*/, float /*new*/> rttMeanWeight;
  std::pair<float /*old*/, float /*new*/> rttVarWeight;
};


} // namespace chunks
} // namespace ndn

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_RTT_ESTIMATOR_HPP
