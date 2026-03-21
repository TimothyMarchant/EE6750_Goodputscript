/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// V2V Cluster Goodput vs PER  —  TCP(BBR) vs QUIC(BBR)
//
// N vehicles randomly placed in a cluster area, all moving in roughly
// the same direction.  Each vehicle opens ONE connection to the next
// vehicle in a ring (0->1, 1->2, ... N-1->0), giving N unicast flows.
// This avoids the ns-3 QUIC module's one-listener-per-node limitation.
//
// X-axis: Packet Error Rate (PER)
// Y-axis: Average goodput per flow (Mbps)
//
// Command-line parameters:
//   --numVehicles     number of vehicles (default 6)
//   --perList         comma-separated PER sweep (default 0,0.005,0.01,0.02,0.05,0.1)
//   --rateMbps        offered load per flow in Mbps (default 1)
//   --packetSize      packet size in bytes (default 512)
//   --simTime         simulation time in seconds (default 40)
//   --measureStart    goodput window start (default 15)
//   --measureEnd      goodput window end   (default 35)
//   --clusterLength   cluster length along highway in metres (default 50)
//   --clusterWidth    cluster width across lanes in metres  (default 5)
//   --speed           mean vehicle speed m/s (default 25 ~ 90 km/h)
//   --speedSpread     max speed deviation m/s (default 3)
//   --seed            RNG seed (default 1)

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include "ns3/quic-module.h"
#include "ns3/quic-client-server-helper.h"
#include "ns3/quic-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2vClusterGoodput");

// -------------------------------------------------------
// Helpers
// -------------------------------------------------------
static std::vector<double>
ParseDoubleList (const std::string &csv)
{
  std::vector<double> out;
  std::stringstream ss (csv);
  std::string item;
  while (std::getline (ss, item, ','))
    if (!item.empty ())
      out.push_back (std::stod (item));
  return out;
}

static void
ApplyPerToDevices (const NetDeviceContainer &devices, double per)
{
  if (per <= 0.0)
    return;
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<WifiNetDevice> wifiNd = DynamicCast<WifiNetDevice> (devices.Get (i));
      NS_ABORT_MSG_IF (wifiNd == nullptr, "Expected WifiNetDevice");
      Ptr<WifiPhy> phy = wifiNd->GetPhy ();
      Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
      em->SetAttribute ("ErrorRate", DoubleValue (per));
      em->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
      phy->SetAttribute ("PostReceptionErrorModel", PointerValue (em));
    }
}

// -------------------------------------------------------
// Run one simulation, return average goodput per flow
// -------------------------------------------------------
static double
RunOne (const std::string &transport,
        double              per,
        uint32_t            numVehicles,
        double              simTime,
        double              measureStart,
        double              measureEnd,
        double              clusterLength,
        double              clusterWidth,
        double              speed,
        double              speedSpread,
        uint32_t            packetSize,
        double              offeredLoadMbps,
        uint32_t            seed)
{
  RngSeedManager::SetSeed (seed);
  RngSeedManager::SetRun (1);

  // 1. Global Protocol Config
  if (transport == "tcp")
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TcpBbr::GetTypeId ()));
    }
  else
    {
      Config::SetDefault ("ns3::QuicL4Protocol::SocketType",
                          TypeIdValue (QuicBbr::GetTypeId ()));
      Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize",
                          UintegerValue (2000000));
      Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize",
                          UintegerValue (2000000));
    }

  // 2. Nodes & Mobility
  NodeContainer nodes;
  nodes.Create (numVehicles);

  Ptr<UniformRandomVariable> rx = CreateObject<UniformRandomVariable> ();
  Ptr<UniformRandomVariable> ry = CreateObject<UniformRandomVariable> ();
  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable> ();
  rx->SetAttribute ("Min", DoubleValue (0.0));
  rx->SetAttribute ("Max", DoubleValue (clusterLength));
  ry->SetAttribute ("Min", DoubleValue (0.0));
  ry->SetAttribute ("Max", DoubleValue (clusterWidth));
  rv->SetAttribute ("Min", DoubleValue (speed - speedSpread));
  rv->SetAttribute ("Max", DoubleValue (speed + speedSpread));

  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  std::vector<double> vSpeeds;
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      posAlloc->Add (Vector (rx->GetValue (), ry->GetValue (), 0.0));
      vSpeeds.push_back (rv->GetValue ());
    }

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetPositionAllocator (posAlloc);
  mobility.Install (nodes);

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      auto m = nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      m->SetVelocity (Vector (vSpeeds[i], 0.0, 0.0));
    }

  // 3. Network Setup — 802.11p / WAVE
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (30.0));
  phy.Set ("TxPowerEnd",   DoubleValue (30.0));

  Wifi80211pHelper wifi = Wifi80211pHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",    StringValue ("OfdmRate12Mbps"),
                                "ControlMode", StringValue ("OfdmRate12Mbps"));

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default ();
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);
  ApplyPerToDevices (devices, per);

  // Internet / QUIC stack
  if (transport == "quic")
    {
      QuicHelper quicStack;
      quicStack.InstallQuic (nodes);
    }
  else
    {
      InternetStackHelper internet;
      internet.Install (nodes);
    }

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Static ARP entries — bypass ARP for WAVE
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<Ipv4L3Protocol> ip = nodes.Get (i)->GetObject<Ipv4L3Protocol> ();
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<ArpCache> arpCache =
          ip->GetInterface (ip->GetInterfaceForDevice (dev))->GetArpCache ();
      for (uint32_t j = 0; j < numVehicles; ++j)
        {
          if (i == j)
            continue;
          ArpCache::Entry *entry = arpCache->Add (ifaces.GetAddress (j));
          entry->SetMacAddress (devices.Get (j)->GetAddress ());
          entry->MarkPermanent ();
        }
    }

  // 4. Applications — Ring topology: node i -> node (i+1) % N
  const uint16_t basePort = 5000;

  for (uint32_t sender = 0; sender < numVehicles; ++sender)
    {
      uint32_t recvr = (sender + 1) % numVehicles;
      uint16_t port  = basePort + sender;

      if (transport == "tcp")
        {
          PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                InetSocketAddress (Ipv4Address::GetAny (), port));
          auto sinkApps = sink.Install (nodes.Get (recvr));
          sinkApps.Start (Seconds (0.5));
          sinkApps.Stop  (Seconds (simTime));

          std::ostringstream rateStr;
          rateStr << offeredLoadMbps << "Mbps";
          OnOffHelper onoff ("ns3::TcpSocketFactory",
                             InetSocketAddress (ifaces.GetAddress (recvr), port));
          onoff.SetAttribute ("DataRate",   DataRateValue (DataRate (rateStr.str ())));
          onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
          onoff.SetAttribute ("OnTime",
                              StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          onoff.SetAttribute ("OffTime",
                              StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          auto clientApps = onoff.Install (nodes.Get (sender));
          clientApps.Start (Seconds (2.0));
          clientApps.Stop  (Seconds (simTime));
        }
      else // quic
        {
          QuicServerHelper server (port);
          auto serverApps = server.Install (nodes.Get (recvr));
          serverApps.Start (Seconds (2.0));
          serverApps.Stop  (Seconds (simTime));

          QuicClientHelper client (ifaces.GetAddress (recvr), port);
          uint32_t maxPkts =
              (uint32_t)((offeredLoadMbps * 1e6 / (packetSize * 8.0)) * simTime * 2);
          client.SetAttribute ("MaxPackets",  UintegerValue (maxPkts));
          client.SetAttribute ("Interval",    TimeValue (MilliSeconds (
              (packetSize * 8.0) / (offeredLoadMbps * 1000.0))));
          client.SetAttribute ("PacketSize",  UintegerValue (packetSize));
          client.SetAttribute ("NumStreams",   UintegerValue (1));

          auto clientApps = client.Install (nodes.Get (sender));
          clientApps.Start (Seconds (10.0));
          clientApps.Stop  (Seconds (simTime));
        }
    }

  // 5. FlowMonitor & Run
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  double totalRxBytes = 0.0;
  uint32_t flowCount  = 0;

  for (const auto &kv : monitor->GetFlowStats ())
    {
      auto t = classifier->FindFlow (kv.first);
      std::cout << "    [DBG] flow " << kv.first << " " << t.sourceAddress
                << " -> " << t.destinationAddress << ":" << t.destinationPort
                << " rxBytes=" << kv.second.rxBytes << "\n";

      if (t.destinationPort >= basePort)
        {
          totalRxBytes += (double)kv.second.rxBytes;
          flowCount++;
        }
    }

  Simulator::Destroy ();
  if (flowCount == 0)
    return 0.0;
  return (totalRxBytes * 8.0) / (flowCount * (measureEnd - measureStart) * 1e6);
}

// -------------------------------------------------------
// main
// -------------------------------------------------------
int
main (int argc, char *argv[])
{
  std::string perListStr  = "0,0.005,0.01,0.02,0.05,0.1";
  std::string FileoutputName = "goodput_";
  std::string outputFolder = "goodputOutputs";
  uint32_t numVehicles    = 6;
  double simTime          = 40.0;
  double measureStart     = 15.0;
  double measureEnd       = 35.0;
  double clusterLength    = 50.0;
  double clusterWidth     = 5.0;
  double speed            = 25.0;
  double speedSpread      = 3.0;
  uint32_t packetSize     = 512;
  double offeredLoadMbps  = 1.0;
  uint32_t seed           = 1;
  
  CommandLine cmd;
  cmd.AddValue("Filename",      "Output file name",                     FileoutputName);
  cmd.AddValue("Foldername",    "Output folder name",                     outputFolder);
  cmd.AddValue ("perList",       "Comma-separated PER list",          perListStr);
  cmd.AddValue ("numVehicles",   "Number of vehicles in cluster",     numVehicles);
  cmd.AddValue ("simTime",       "Simulation time (s)",               simTime);
  cmd.AddValue ("measureStart",  "Goodput measurement start (s)",     measureStart);
  cmd.AddValue ("measureEnd",    "Goodput measurement end (s)",       measureEnd);
  cmd.AddValue ("clusterLength", "Cluster length along highway (m)",  clusterLength);
  cmd.AddValue ("clusterWidth",  "Cluster width across lanes (m)",    clusterWidth);
  cmd.AddValue ("speed",         "Mean vehicle speed (m/s)",          speed);
  cmd.AddValue ("speedSpread",   "Max speed deviation (m/s)",         speedSpread);
  cmd.AddValue ("packetSize",    "Packet size in bytes",              packetSize);
  cmd.AddValue ("rateMbps",      "Offered load per flow (Mbps)",      offeredLoadMbps);
  cmd.AddValue ("seed",          "RNG seed",                          seed);
  cmd.Parse (argc, argv);


  std::string seedString  = std::to_string(seed);

  try {
    std::filesystem::create_directory(outputFolder);
  }
  catch (int e){
    std::cout << e;
    std::cout << "Folder exists already";
  }
  auto pers = ParseDoubleList (perListStr);
  NS_ABORT_MSG_IF (pers.empty (), "perList is empty");
  NS_ABORT_MSG_IF (numVehicles < 2, "Need at least 2 vehicles");

  uint32_t numFlows = numVehicles; // ring: one flow per vehicle

  std::cout << "V2V Cluster Goodput vs PER  |  802.11p/WAVE  |  TCP(BBR) vs QUIC(BBR)\n";
  std::cout << "numVehicles=" << numVehicles
            << "  flows=" << numFlows << " (ring)"
            << "  offeredLoad=" << offeredLoadMbps << " Mbps/flow"
            << "  packetSize=" << packetSize << " B\n";
  std::cout << "cluster=" << clusterLength << "x" << clusterWidth << " m"
            << "  speed=" << speed << " +/-" << speedSpread << " m/s"
            << "  simTime=" << simTime << " s"
            << "  measure=[" << measureStart << "," << measureEnd << "] s\n\n";

  std::ofstream tcpDat (outputFolder + "/"  + seedString + "_" +  FileoutputName+"tcp.dat");
  std::ofstream quicDat (outputFolder + "/" + seedString + "_" + FileoutputName+"quic.dat");
  tcpDat  << "#per avg_goodput_mbps_per_flow\n";
  quicDat << "#per avg_goodput_mbps_per_flow\n";

  for (double per : pers)
    {
      std::cout << "PER=" << per << " : TCP(BBR)..." << std::flush;
      double tcpGp = RunOne ("tcp", per, numVehicles, simTime, measureStart,
                             measureEnd, clusterLength, clusterWidth, speed,
                             speedSpread, packetSize, offeredLoadMbps, seed);
      std::cout << " " << tcpGp << " Mbps\n" << std::flush;

      std::cout << "PER=" << per << " : QUIC(BBR)..." << std::flush;
      double quicGp = RunOne ("quic", per, numVehicles, simTime, measureStart,
                              measureEnd, clusterLength, clusterWidth, speed,
                              speedSpread, packetSize, offeredLoadMbps, seed);
      std::cout << " " << quicGp << " Mbps\n\n" << std::flush;

      tcpDat  << std::fixed << std::setprecision (6) << per << " " << tcpGp  << "\n";
      quicDat << std::fixed << std::setprecision (6) << per << " " << quicGp << "\n";
    }

  tcpDat.close ();
  quicDat.close ();

  // Gnuplot script
  std::ofstream gp ("goodput.plt");
  gp << "set terminal pngcairo size 1000,600\n"
     << "set output 'goodput.png'\n"
     << "set title 'V2V Cluster Avg Goodput vs PER (" << numVehicles
     << " vehicles, " << numFlows << " flows, ring) — TCP(BBR) vs QUIC(BBR)'\n"
     << "set xlabel 'Packet Error Rate (PER)'\n"
     << "set ylabel 'Avg Goodput per Flow (Mbps)'\n"
     << "set grid\n"
     << "set key left top\n"
     << "plot 'goodput_tcp.dat'  using 1:2 with linespoints title 'TCP (BBR)', \\\n"
     << "     'goodput_quic.dat' using 1:2 with linespoints title 'QUIC (BBR)'\n";
  gp.close ();

  std::cout << "Output: goodput_tcp.dat  goodput_quic.dat  goodput.plt\n";
  std::cout << "To plot: gnuplot goodput.plt\n";

  return 0;
}
