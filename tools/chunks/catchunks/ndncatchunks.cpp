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

#include "core/version.hpp"
#include "options.hpp"
#include "consumer.hpp"
#include "discover-version-fixed.hpp"
#include "discover-version-iterative.hpp"
#include "../chunks-tracepoint.hpp"

#include <ndn-cxx/security/validator-null.hpp>

namespace ndn {
namespace chunks {

void
handleSIGINT(const boost::system::error_code& errorCode, Consumer& consumer) {
  if (errorCode == boost::asio::error::operation_aborted) {
    return;
  }

  consumer.cancel();
  tracepoint(chunksLog, cat_stopped, 4);
}

static int
main(int argc, char** argv)
{
  std::string programName(argv[0]);
  PipelineInterests::Options options;
  std::string discoverType("fixed");
  int maxRetriesAfterVersionFound(1);
  std::string uri;


  bool printStat = false;
  uint64_t randomWaitMax = 0;
  bool startWait = false;
  bool noDiscovery = false;

  namespace po = boost::program_options;
  po::options_description visibleDesc("Options");
  visibleDesc.add_options()
    ("help,h",      "print this help message and exit")
    ("discover-version,d",  po::value<std::string>(&discoverType)->default_value(discoverType),
                            "version discovery algorithm to use; valid values are: 'fixed', 'iterative'")
    ("fresh,f",     po::bool_switch(&options.mustBeFresh), "only return fresh content")
    ("lifetime,l",  po::value<uint64_t>()->default_value(options.interestLifetime.count()),
                    "lifetime of expressed Interests, in milliseconds")
    ("pipelineStart,p",  po::value<size_t>(&options.startPipelineSize)->default_value(options.startPipelineSize),
                    "initial max size of the Interest pipeline")
    ("pipelineMax,m",  po::value<size_t>(&options.maxPipelineSize)->default_value(options.maxPipelineSize),
                    "maximum size of the Interest pipeline (0 = same as start pipeline size)")
    ("timeoutBeforeReset,R",  po::value<size_t>(&options.nTimeoutBeforeReset)->default_value(options.nTimeoutBeforeReset),
                    "number of consecutive timeouts to reset the rtt estimator")
    ("rtoMultiplierReset,M",  po::bool_switch(&options.rtoMultiplierReset),
                    "turn on reset RTO multiplier on Data received (Now is half not a full reset)")
    ("retries,r",   po::value<int>(&options.maxRetriesOnTimeoutOrNack)->default_value(options.maxRetriesOnTimeoutOrNack),
                    "maximum number of retries in case of Nack or timeout (-1 = no limit)")
    ("retries-iterative,i", po::value<int>(&maxRetriesAfterVersionFound)->default_value(maxRetriesAfterVersionFound),
                            "number of timeouts that have to occur in order to confirm a discovered Data "
                            "version as the latest one (only applicable to 'iterative' version discovery)")
    ("verbose,v",   po::bool_switch(&options.isVerbose), "turn on verbose output")
    ("version,V",   "print program version and exit")
    ("printStat,S", po::bool_switch(&printStat), "turn on statistics output")
    ("randomWait,w",  po::value<uint64_t>(&randomWaitMax)->default_value(randomWaitMax),
                    "maximum wait time before sending an interest")
    ("startWait,W",  po::bool_switch(&startWait), "add delay only to the first interest of each pipe")
    ("noDiscovery,k",  po::bool_switch(&noDiscovery), "disable discovery, the name should have a version number")
    ("windowCutMultiplier,c",  po::value<float>(&options.windowCutMultiplier)->default_value(options.windowCutMultiplier),
                                    "window cut multiplier")
    ("slowStartThreshold,t",  po::value<size_t>(&options.slowStartThreshold)->default_value(options.slowStartThreshold),
                              "slow start threshold (0 = no threshold)")
    ;

  po::options_description hiddenDesc("Hidden options");
  hiddenDesc.add_options()
    ("ndn-name,n", po::value<std::string>(&uri), "NDN name of the requested content");

  po::positional_options_description p;
  p.add("ndn-name", -1);

  po::options_description optDesc("Allowed options");
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).positional(p).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }

  if (vm.count("help") > 0) {
    std::cout << "Usage: " << programName << " [options] ndn:/name" << std::endl;
    std::cout << visibleDesc;
    return 0;
  }

  if (vm.count("version") > 0) {
    std::cout << "ndncatchunks " << tools::VERSION << std::endl;
    return 0;
  }

  if (vm.count("ndn-name") == 0) {
    std::cerr << "Usage: " << programName << " [options] ndn:/name" << std::endl;
    std::cerr << visibleDesc;
    return 2;
  }

  Name prefix(uri);
  if (discoverType == "fixed" && (prefix.empty() || !prefix[-1].isVersion())) {
    std::cerr << "ERROR: The specified name must contain a version component when using "
                 "fixed version discovery" << std::endl;
    return 2;
  }

  if (options.startPipelineSize < 1 || options.startPipelineSize > 65536) {
    std::cerr << "ERROR: start pipeline size must be between 1 and 65536" << std::endl;
    return 2;
  }

  if (options.maxPipelineSize == 0)
    options.maxPipelineSize = options.startPipelineSize;

  if (options.maxPipelineSize < options.startPipelineSize || options.maxPipelineSize > 65536) {
    std::cerr << "ERROR: max pipeline size must be between pipelineStart and 65536" << std::endl;
    return 2;
  }

  if (options.maxRetriesOnTimeoutOrNack < -1 || options.maxRetriesOnTimeoutOrNack > 1024) {
    std::cerr << "ERROR: retries value must be between -1 and 1024" << std::endl;
    return 2;
  }

  if (maxRetriesAfterVersionFound < 0 || maxRetriesAfterVersionFound > 1024) {
    std::cerr << "ERROR: retries iterative value must be between 0 and 1024" << std::endl;
    return 2;
  }

  if (options.windowCutMultiplier < 0 || options.windowCutMultiplier > 1) {
    std::cerr << "ERROR: window cut multiplier value must be between 0 and 1" << std::endl;
    return 2;
  }

  options.interestLifetime = time::milliseconds(vm["lifetime"].as<uint64_t>());

  try {
    Face face;
    boost::asio::signal_set m_signalSetInt(face.getIoService(), SIGINT);


    unique_ptr<DiscoverVersion> discover;
    if (discoverType == "fixed") {
      discover = make_unique<DiscoverVersionFixed>(prefix, face, options);
    }
    else if (discoverType == "iterative") {
      DiscoverVersionIterative::Options optionsIterative(options);
      optionsIterative.maxRetriesAfterVersionFound = maxRetriesAfterVersionFound;
      discover = make_unique<DiscoverVersionIterative>(prefix, face, optionsIterative);
    }
    else {
      std::cerr << "ERROR: discover version type not valid" << std::endl;
      return 2;
    }

    ValidatorNull validator;
    Consumer consumer(face, validator, options.isVerbose, std::cout, printStat);
    m_signalSetInt.async_wait(bind(ndn::chunks::handleSIGINT, _1, std::ref(consumer)));


    PipelineInterests pipeline(face, options, randomWaitMax, startWait);

    BOOST_ASSERT(discover != nullptr);

    tracepoint(chunksLog, cat_started, options.startPipelineSize, options.maxPipelineSize, options.interestLifetime.count(),
               options.maxRetriesOnTimeoutOrNack, options.mustBeFresh, startWait, options.slowStartThreshold,
               options.nTimeoutBeforeReset, options.windowCutMultiplier, options.rtoMultiplierReset);

    consumer.run(*discover, pipeline, noDiscovery);
    m_signalSetInt.cancel();
  }
  catch (const Consumer::ApplicationNackError& e) {
    tracepoint(chunksLog, cat_stopped, 3);
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 3;
  }
  catch (const std::exception& e) {
    tracepoint(chunksLog, cat_stopped, 1);
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }

  tracepoint(chunksLog, cat_stopped, 0);
  return 0;
}

} // namespace chunks
} // namespace ndn

int
main(int argc, char** argv)
{
  return ndn::chunks::main(argc, argv);
}
