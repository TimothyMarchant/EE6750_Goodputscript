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


#include "ns3/point-to-point-module.h"


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/command-line.h"

#include "ns3/quic-bbr.h"
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
//Parse list of integers
static std::vector<uint32_t>
ParseUintList (const std::string &IntList)
{

  std::vector<uint32_t> out;
  std::stringstream ss (IntList);
  std::string item;
  while (std::getline (ss, item, ','))
    if (!item.empty ())
      out.push_back (std::stod (item));
  return out;
}
/// Compute the P95 value from a vector of doubles (modifies input via sort)
static double
Percentile95 (std::vector<double> &vals)
{
  if (vals.empty ())
    return 0.0;
  std::sort (vals.begin (), vals.end ());
  // Use nearest-rank method
  size_t idx = (size_t) std::ceil (0.95 * vals.size ()) - 1;
  if (idx >= vals.size ())
    idx = vals.size () - 1;
  return vals[idx];
}
//Get average value from a vector of doubles
static double
DataAverage (std::vector<double> &vals)
{
  if (vals.empty ())
    {
      return 0.0;
    }
  double sum = 0;
  int length = vals.size ();
  for (int i = 0; i < length; i++)
    {
      sum += vals[i];
    }
  double average = (double) sum / length;
  return average;
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
//strings should be formatted already. delimter=,
static void
CreateCSVFile (const std::string FileName)
{
  const std::string ColumnNames = "Vehicles,Speed,PER,AverageLatency,P95Latency,AverageJitter,Goodput";
  try
    {
      std::ofstream File;

      File.open (FileName);
      File << ColumnNames << std::endl;
      File.close ();
    }
  catch (int e)
    {
      std::cout << "Create error\n";
      std::cout << e << std::endl;
    }
}
static void
WriteToCSVFile (double *Row, const std::string FileName)
{
  std::fstream streamwriter;

  try
    {
      streamwriter.open (FileName, std::ios::app);
      for (int i = 0; i < 6; i++)
        {
          streamwriter << Row[i];
          streamwriter << ",";
        }
      streamwriter << Row[6] << std::endl;
      streamwriter.close ();
    }
  catch (int e)
    {
      std::cout << "Write Error\n";
      std::cout << e << std::endl;
    }
}
// -------------------------------------------------------
// Run one simulation, return average goodput per flow
// -------------------------------------------------------
static double
RunOne (const std::string &transport, double per, uint32_t numVehicles, double simTime,
        double measureStart, double measureEnd, double clusterLength, double clusterWidth,
        double speed, double speedSpread, uint32_t packetSize, double offeredLoadMbps,
        uint32_t seed, uint32_t verbose, uint32_t QUICStreams, std::string FileName, std::string PCAPFileName)
{
  ns3::PointToPointHelper pointToPoint;
  //pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  //pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  RngSeedManager::SetSeed (seed);
  RngSeedManager::SetRun (1);
  //taken from the example.  If set too high the program breaks.  Set to 500Kbps, program doesn't break that way.  It does at 1Mbps.
  Config::SetDefault ("ns3::TcpSocketState::MaxPacingRate", StringValue ("500Kbps"));
  Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (1));
  // 1. Global Protocol Config
  if (transport == "tcp")
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpBbr::GetTypeId ()));
    }
  else
    {
      Config::SetDefault ("ns3::QuicL4Protocol::SocketType", TypeIdValue (QuicBbr::GetTypeId ()));
      //required.  High buffer size.  Buffer overflow breaks the simulation and causes an infinite wait for whatever reason.
      Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize", UintegerValue (40000000));
      Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize", UintegerValue (40000000));
      Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize", UintegerValue (40000000));

      Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize", UintegerValue (40000000));
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
  phy.Set ("TxPowerEnd", DoubleValue (30.0));

  Wifi80211pHelper wifi = Wifi80211pHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",
                                StringValue ("OfdmRate12Mbps"), "ControlMode",
                                StringValue ("OfdmRate12Mbps"));

  

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
  NetDeviceContainer PCAPdevices;
  //pointToPoint.Install(nodes);

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default ();
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);
  ApplyPerToDevices (devices, per);

  std::cout << "RAN";
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  //Ipv4InterfaceContainer interfaces = ipv4.Assign (PCAPdevices);

  // Static ARP entries — bypass ARP for WAVE
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<Ipv4L3Protocol> ip = nodes.Get (i)->GetObject<Ipv4L3Protocol> ();
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<ArpCache> arpCache = ip->GetInterface (ip->GetInterfaceForDevice (dev))->GetArpCache ();
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
      uint16_t port = basePort + sender;

      if (transport == "tcp")
        {
          PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                 InetSocketAddress (Ipv4Address::GetAny (), port));
          auto sinkApps = sink.Install (nodes.Get (recvr));
          sinkApps.Start (Seconds (0.5));
          sinkApps.Stop (Seconds (simTime-1.0));

          std::ostringstream rateStr;
          rateStr << offeredLoadMbps << "Mbps";
          OnOffHelper onoff ("ns3::TcpSocketFactory",
                             InetSocketAddress (ifaces.GetAddress (recvr), port));
          onoff.SetAttribute ("DataRate", DataRateValue (DataRate (rateStr.str ())));
          onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          auto clientApps = onoff.Install (nodes.Get (sender));
          clientApps.Start (Seconds (2.0));
          clientApps.Stop (Seconds (simTime));
        }
      else // quic
        {
          QuicServerHelper server (port);
          auto serverApps = server.Install (nodes.Get (recvr));
          serverApps.Start (Seconds (0.5));
          serverApps.Stop (Seconds (simTime-1.0));

          QuicClientHelper client (ifaces.GetAddress (recvr), port);
          uint32_t maxPkts =
              (uint32_t) ((offeredLoadMbps * 1e6 / (packetSize * 8.0)) * simTime * 2);
          client.SetAttribute ("MaxPackets", UintegerValue (maxPkts));
          client.SetAttribute ("Interval", TimeValue (MilliSeconds ((packetSize * 8.0) /
                                                                    (offeredLoadMbps * 1000.0))));
          client.SetAttribute ("PacketSize", UintegerValue (packetSize));
          client.SetAttribute ("NumStreams", UintegerValue (QUICStreams));
          auto clientApps = client.Install (nodes.Get (sender));
          clientApps.Start (Seconds (2.0));
          clientApps.Stop (Seconds (simTime));
        }
    }

  // 5. FlowMonitor & Run

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  Ptr<Ipv4FlowClassifier> classifier = StaticCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  double totalRxBytes = 0.0;
  double TotalJitter = 0.0;
  std::vector<double> flowAvgDelaysMs;

  uint32_t flowCount = 0;
  //pointToPoint.EnablePcapAll (PCAPFileName);
  phy.EnablePcap(PCAPFileName, devices);
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Simulator::Destroy ();

  for (const auto &kv : monitor->GetFlowStats ())
    {
      auto t = classifier->FindFlow (kv.first);
      if (verbose)
        {
          std::cout << "    [DBG] flow " << kv.first << " " << t.sourceAddress << " -> "
                    << t.destinationAddress << ":" << t.destinationPort
                    << " rxBytes=" << kv.second.rxBytes << std::endl;
        }
      if (t.destinationPort >= basePort)
        {
          const auto &stats = kv.second;
          if (stats.rxPackets == 0)
            continue;

          // Average delay for this flow in milliseconds
          double avgDelayMs = stats.delaySum.GetMilliSeconds () / (double) stats.rxPackets;
          flowAvgDelaysMs.push_back (avgDelayMs);

          TotalJitter += (double) stats.jitterSum.GetMilliSeconds () / (double) stats.rxPackets;
          totalRxBytes += (double) kv.second.rxBytes;

          flowCount++;
        }
    }
  double AverageDelay = DataAverage (flowAvgDelaysMs);
  double AverageJitter = TotalJitter / flowCount;
  double p95 = Percentile95 (flowAvgDelaysMs);
  double goodput = (totalRxBytes * 8.0) / (flowCount * (measureEnd - measureStart) * 1e6);
  //Row
  double Row[] = {(double) numVehicles, speed, per, AverageDelay, p95, AverageJitter, goodput};
  WriteToCSVFile (Row, FileName);

  std::cout << "flowcount: " << flowCount << std::endl;
  if (flowCount == 0)
    return 0.0;
  return (totalRxBytes * 8.0) / (flowCount * (measureEnd - measureStart) * 1e6);
}
#define QUICSelect 1
#define TCPSelect 0
// -------------------------------------------------------
// main
// -------------------------------------------------------
int
main (int argc, char *argv[])
{
  std::string perListStr = "0,0.005,0.01,0.02,0.05,0.1";
  std::string FileoutputName = "goodput";
  std::string outputFolder = "goodputOutputs";
  std::string numVehiclesStr = "6";
  std::string speedListStr = "25.0";

  uint32_t streams = 1;
  double simTime = 40.0;
  double measureStart = 15.0;
  double measureEnd = 35.0;
  double clusterLength = 50.0;
  double clusterWidth = 5.0;
  double speedSpread = 3.0;
  uint32_t packetSize = 512;
  double offeredLoadMbps = 1.0;
  uint32_t seed = 1;
  uint32_t verbose = 1;
  uint32_t overWrite = 1;
  uint32_t TCPOrQUIC = 1;

  CommandLine cmd;
  cmd.AddValue ("Filename", "Output file name", FileoutputName);
  cmd.AddValue ("Foldername", "Output folder name", outputFolder);
  cmd.AddValue ("perList", "Comma-separated PER list", perListStr);
  cmd.AddValue ("numVehicles", "Number of vehicles in cluster", numVehiclesStr);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("measureStart", "Goodput measurement start (s)", measureStart);
  cmd.AddValue ("measureEnd", "Goodput measurement end (s)", measureEnd);
  cmd.AddValue ("clusterLength", "Cluster length along highway (m)", clusterLength);
  cmd.AddValue ("clusterWidth", "Cluster width across lanes (m)", clusterWidth);
  cmd.AddValue ("speedList", "Mean vehicle speed (m/s)", speedListStr);
  cmd.AddValue ("speedSpread", "Max speed deviation (m/s)", speedSpread);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("rateMbps", "Offered load per flow (Mbps)", offeredLoadMbps);
  cmd.AddValue ("seed", "RNG seed", seed);
  cmd.AddValue ("verbose", "Is output verbose", verbose);
  cmd.AddValue ("Streams", "Number of QUIC Streams", streams);
  cmd.AddValue ("overWrite", "Overwrite existing csv files", overWrite);
  cmd.AddValue ("TCPOrQUIC", "TO select which protocol to use", TCPOrQUIC);
  cmd.Parse (argc, argv);

  std::cout << perListStr;
  std::string seedString = std::to_string (seed);
  std::string QUICOUTPUTFOLDER="QUIC";
  std::string TCPOUTPUTFOLDER="TCP";
  std::string PCAPFolder="PCAP";
  try
    {
      std::filesystem::create_directory (outputFolder);
      std::filesystem::create_directory (outputFolder+"/"+QUICOUTPUTFOLDER);
      std::filesystem::create_directory (outputFolder+"/"+TCPOUTPUTFOLDER);
      std::filesystem::create_directory (outputFolder+"/"+ QUICOUTPUTFOLDER + "/" + PCAPFolder);
      std::filesystem::create_directory (outputFolder+"/"+ TCPOUTPUTFOLDER + "/" + PCAPFolder);
    }
  catch (int e)
    {
      std::cout << e;
      std::cout << "Folder exists already";
    }
  auto numVehicles = ParseUintList (numVehiclesStr);
  auto speedList = ParseDoubleList (speedListStr);
  auto pers = ParseDoubleList (perListStr);
  NS_ABORT_MSG_IF (pers.empty (), "perList is empty");
  NS_ABORT_MSG_IF (numVehicles[0] < 2, "Need at least 2 vehicles");

  std::string QUICCSV = outputFolder + "/" + QUICOUTPUTFOLDER + "/" + seedString + "_" + FileoutputName + "_QUIC.csv";
  std::string TCPCSV = outputFolder + "/" + TCPOUTPUTFOLDER + "/" + seedString + "_" + FileoutputName + "_TCP.csv";

  std::string PCAPNameQUIC = outputFolder + "/" + QUICOUTPUTFOLDER + "/" + PCAPFolder + "/" + seedString + "_" + FileoutputName + "_QUIC";
  std::string PCAPNameTCP = outputFolder + "/" + TCPOUTPUTFOLDER + "/" + PCAPFolder + "/" + seedString + "_" + FileoutputName + "_TCP";


  if (overWrite == 1)
    {
      CreateCSVFile (TCPCSV);
      CreateCSVFile (QUICCSV);
    }

  for (double per : pers)
    {
      for (uint32_t VehicleCount : numVehicles)
        {
          for (double speed : speedList)
            {
              uint32_t numFlows = VehicleCount; // ring: one flow per vehicle
              if (verbose)
                {
                  std::cout
                      << "V2V Cluster Goodput vs PER  |  802.11p/WAVE  |  TCP(BBR) vs QUIC(BBR)\n";
                  std::cout << "numVehicles=" << VehicleCount << "  flows=" << numFlows << " (ring)"
                            << "  offeredLoad=" << offeredLoadMbps << " Mbps/flow"
                            << "  packetSize=" << packetSize << " B\n";
                  std::cout << "cluster=" << clusterLength << "x" << clusterWidth << " m"
                            << "  speed=" << speed << " +/-" << speedSpread << " m/s"
                            << "  simTime=" << simTime << " s"
                            << "  measure=[" << measureStart << "," << measureEnd << "] s\n\n";
                }
              double tcpGp = 0;
              double quicGp = 0;
              if (1)
                {
                  if (verbose)
                    {
                      std::cout << "PER=" << per << " : TCP(BBR)..." << std::flush;
                    }
                  try
                    {
                      tcpGp = RunOne ("tcp", per, VehicleCount, simTime, measureStart, measureEnd,
                                      clusterLength, clusterWidth, speed, speedSpread, packetSize,
                                      offeredLoadMbps, seed, verbose, streams, TCPCSV, PCAPNameTCP);
                    }
                  catch (int e)
                    {
                      std::cout << e;
                      tcpGp = -1; //error value
                    }
                  if (verbose)
                    {
                      std::cout << " " << tcpGp << " Mbps\n" << std::flush;
                    }
                }
              if (1)
                {
                  if (verbose)
                    {

                      std::cout << "PER=" << per << " : QUIC(BBR)..." << std::flush;
                    }
                  try
                    {
                      quicGp = RunOne ("quic", per, VehicleCount, simTime, measureStart, measureEnd,
                                       clusterLength, clusterWidth, speed, speedSpread, packetSize,
                                       offeredLoadMbps, seed, verbose, streams, QUICCSV, PCAPNameQUIC);
                    }
                  catch (int e)
                    {
                      std::cout << e;
                      quicGp = -1; //error value
                    }
                  if (verbose)
                    {
                      std::cout << " " << quicGp << " Mbps\n\n" << std::flush;
                    }
                }
            }
        }
    }

  return 0;
}
