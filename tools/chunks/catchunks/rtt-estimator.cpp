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

#include "rtt-estimator.hpp"
#include "data-fetcher.hpp"

namespace ndn {
namespace chunks {

RttEstimator::RttEstimator()
{
  rttMeanWeight.first = 0.3;  // Old value
  rttMeanWeight.second = 0.7; // New value

  rttVarWeight.first = 0.125;  // Old value
  rttVarWeight.second = 0.875; // New value

  reset();

  m_nSamples = 5;
}

float
RttEstimator::addRttMeasurement(const shared_ptr<DataFetcher>& df)
{
  float rtt = -1;
  if (df->m_transmissionTimes.size() == 1) { // No retry
    rtt = (time::duration_cast<time::milliseconds> (time::steady_clock::now() - df->m_transmissionTimes[0])).count();

    if (m_rttMinCalc == -1) {
      m_rttMinCalc = rtt;
      //tracepoint(strategyLog, rtt_min_calc, m_rttMinCalc);
    }
    else if (rtt < m_rttMinCalc) {
      m_rttMinCalc = rtt;
      //tracepoint(strategyLog, rtt_min_calc, m_rttMinCalc);
    }

    //NFD_LOG_DEBUG("Rtt min calc: " << m_rttMinCalc);
  }
  else if (df->m_transmissionTimes.size() > 1) { // At least 1 retry
    for (int i = df->m_transmissionTimes.size(); i > 0; i--) {
      rtt = (time::duration_cast<time::milliseconds> (time::steady_clock::now() - df->m_transmissionTimes[i - 1])).count();
      if (m_rttMinCalc != -1 && rtt >= m_rttMinCalc)
        break;
      else if (m_rttMinCalc == -1 && rtt >= m_rttMin)
        break;
    }
  }
  else
    return -1; // This should not happen
  float rttOriginal = rtt;

  if (m_rttMinCalc == -1 && rtt < m_rttMin) {
    //tracepoint(strategyLog, rtt_min, rtt);
    rtt = m_rttMin;
  }
  else if (m_rttMinCalc != -1 && rtt < m_rttMinCalc) {
    //tracepoint(strategyLog, rtt_min, rtt);
    rtt = m_rttMinCalc;
  }

  if (rtt > m_rttMax) {
    //tracepoint(strategyLog, rtt_max, rtt);
    rtt = m_rttMax;
  }

  while (m_oldRtt.size() > m_nSamples) {
    m_oldRtt.erase(m_oldRtt.begin());
  }
  m_oldRtt.push_back(rtt);

  float newMean = m_oldRtt[0];
  float newVar = m_oldRtt[0] / 2;
  for(uint32_t i = 1; i < m_oldRtt.size(); i++) {
    newVar = (newVar * rttVarWeight.first) + (std::abs(m_oldRtt[i] - newMean) * rttVarWeight.second);
    newMean = (newMean * rttMeanWeight.first) + (m_oldRtt[i] * rttMeanWeight.second);
  }

  m_lastRtt = rtt;
  m_rttMean = newMean;
  m_rttVar = newVar;
  //m_rtoMulti = 1;

  return rttOriginal;
}

float
RttEstimator::getRTO() const
{
  if (m_rttMean == -1)
    return -1;

  return m_rtoMulti * (m_rttMean + (m_rttVar * 4));
}

float
RttEstimator::getRttMean() const
{
  return m_rttMean;
}

float
RttEstimator::getRttVar() const
{
  return m_rttVar;
}

float
RttEstimator::incrementRtoMultiplier()
{
  if (m_rtoMulti > 16)  // max 32
    return m_rtoMulti;

  m_rtoMulti *= 2;

  return m_rtoMulti;
}

float
RttEstimator::decrementRtoMultiplier()
{
  if (m_rtoMulti < 2) // min 1
    return m_rtoMulti;

  m_rtoMulti /= 2;

  return m_rtoMulti;
}

float
RttEstimator::getRtoMultiplier()
{
  return m_rtoMulti;
}

void
RttEstimator::reset()
{
  m_rttMean = -1;
  m_rttVar = -1;
  m_lastRtt = -1;
  m_rttMulti = 2;
  m_rttMin = 10;
  m_rttMax = 2000;
  m_rtt0 = 250;
  m_rttMinCalc = -1;
  m_rtoMulti = 1;
}


} // namespace chunks
} // namespace ndn
