//       Network topology
//
//       n0 -------------- n1
//            1Mbps-10ms

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/random-variable-stream.h" 
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpComparision");

Ptr<PacketSink> tcpSink;

void
ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon,Gnuplot2dDataset DataSet)
{
  double localThrou = 0;
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
    {
      Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
      std::cout << "Flow ID			: "<< stats->first << " ; " << fiveTuple.sourceAddress << " -----> " << fiveTuple.destinationAddress << std::endl;
      std::cout << "Tx Packets = " << stats->second.txPackets << std::endl;
      std::cout << "Rx Packets = " << stats->second.rxPackets << std::endl;
      std::cout << "Duration		: "<< (stats->second.timeLastRxPacket.GetSeconds () - stats->second.timeFirstTxPacket.GetSeconds ()) << std::endl;
      std::cout << "Last Received Packet	: "<< stats->second.timeLastRxPacket.GetSeconds () << " Seconds" << std::endl;
      std::cout << "Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds () - stats->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps" << std::endl;
      localThrou = stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds () - stats->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;
      if (stats->first == 1)
        {
          DataSet.Add ((double)Simulator::Now ().GetSeconds (),(double) localThrou);
        }
      std::cout << "---------------------------------------------------------------------------" << std::endl;
    }
  Simulator::Schedule (Seconds (10),&ThroughputMonitor, fmhelper, flowMon,DataSet);
  flowMon->SerializeToXmlFile ("ThroughputMonitor.xml", true, true);
}


int
main (int argc, char *argv[])
{

  uint8_t sendersCount = 4;
  uint8_t receiversCount = 2;

  uint32_t maxBytes = 0;
  double error = 0.000001;
  // double duration = 100.0;

  // Allow users to pick any of the defaults at run-time, via command-line arguments
  CommandLine cmd;
  cmd.AddValue ("maxBytes", "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("error", "Packet error rate", error);

  cmd.Parse (argc, argv);

  //NodeContainer nodes;
  //nodes.Create (2);

  // Explicitly create the nodes in the topology.
  NS_LOG_INFO ("Create nodes");

  NodeContainer loadBalancer; 
  loadBalancer.Create (1);
  NodeContainer senders; 
  senders.Create (sendersCount);
  NodeContainer receivers;
  receivers.Create (receiversCount);

  loadBalancer.Get (0)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  for (uint8_t i = 0; i < sendersCount; ++i) {
      senders.Get (i)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  }
  for (uint8_t i = 0; i < receiversCount; ++i) {
      receivers.Get (i)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  }

  // Set symmetric losses to 50 dB
  NS_LOG_INFO ("Set nodes symmetric losses");

  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (60); // set default loss to 60 dB (no link)
  for (uint8_t i = 0; i < sendersCount; ++i) {
    lossModel->SetLoss (senders.Get (i)->GetObject<MobilityModel> (), loadBalancer.Get (0)->GetObject<MobilityModel> (), 50);
  }
  for (uint8_t i = 0; i < receiversCount; ++i) {
    lossModel->SetLoss (receivers.Get (i)->GetObject<MobilityModel> (), loadBalancer.Get (0)->GetObject<MobilityModel> (), 50);
  }
  
  NS_LOG_INFO ("Create channels");

  // PointToPointHelper pointToPoint;
  // pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  // pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  // pointToPoint.SetQueue ("ns3::DropTailQueue");
  
  // NetDeviceContainer devices;
  // devices = pointToPoint.Install (nodes);

  // Explicitly create the wi-fi link in the topology.
  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  //wifi.SetRemoteStationManager ("ns3::" + wifiManager + "WifiManager");
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel (wifiChannel);
  WifiMacHelper staWifiMac, apWifiMac;
  staWifiMac.SetType ("ns3::StaWifiMac");
  apWifiMac.SetType ("ns3::ApWifiMac");
  NetDeviceContainer sendersDevices = wifi.Install (wifiPhy, staWifiMac, senders);
  NetDeviceContainer loadBalancerDevice = wifi.Install (wifiPhy, apWifiMac, loadBalancer);
  NetDeviceContainer receiversDevices = wifi.Install (wifiPhy, staWifiMac, receivers);

  // Install the internet stack on the nodes.
  InternetStackHelper internet;
  internet.Install (senders);
  internet.Install (loadBalancer);
  internet.Install (receivers);
  Ipv4AddressHelper sendersIpv4, loadBalancerIpv4, receiversIpv4;  

  
  // Add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses");

  sendersIpv4.SetBase ("10.0.0.0", "255.255.255.0");
  loadBalancerIpv4.SetBase ("10.0.1.0", "255.255.255.0");
  receiversIpv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer sendersAddresses = sendersIpv4.Assign (sendersDevices);
  Ipv4InterfaceContainer loadBalancerAddress = loadBalancerIpv4.Assign (loadBalancerDevice);
  Ipv4InterfaceContainer recieversAddresses = receiversIpv4.Assign (receiversDevices);

  // Configure IPv4 address
  Ipv4Address addr;

  for(int i = 0 ; i < sendersCount; i++) {
    addr = sendersAddresses.GetAddress(i);
    std::cout << "Sender node #" << i+1 << "\t "<< "IP Address "<< addr << std::endl; 
  }
 
  for(int i = 0 ; i < receiversCount; i++) {
    addr = recieversAddresses.GetAddress(i);
    std::cout << "Reciver node #" << i+1 << "\t "<< "IP Address "<< addr << std::endl; 
  }


  addr = loadBalancerAddress.GetAddress(0);
  std::cout << "Load balancer node " << "\t "<< "IP Address "<< addr << std::endl; 


  //  Create error model on receiver.
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (error));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Install the internet stack on the nodes.
  // InternetStackHelper internet;
  // internet.Install (nodes);
  
  // Ipv4AddressHelper ipv4;
  // ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  // Ipv4InterfaceContainer ipv4Container = ipv4.Assign (devices);

  // NS_LOG_INFO ("Create Applications");

  // Create a BulkSendApplication and install it on node 0.
  // uint16_t port = 1102;

  // BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress (ipv4Container.GetAddress (1), port));

  // Set the amount of data to send in bytes. Zero is unlimited.
  // source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  // ApplicationContainer sourceApps = source.Install (nodes.Get (0));

  // sourceApps.Start (Seconds (0.0));
  // sourceApps.Stop (Seconds (duration));

  // Create a PacketSinkApplication and install it on node 1.
  // PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  // ApplicationContainer sinkApps = sink.Install (nodes.Get (1));


  // sinkApps.Start (Seconds (0.0));
  // sinkApps.Stop (Seconds (duration));

  // Just for debug (in our code).
  // tcpSink = DynamicCast<PacketSink> (sinkApps.Get (0));

  NS_LOG_INFO ("Run Simulation");

  // std::string fileNameWithNoExtension = "FlowVSThroughput_";
  // std::string mainPlotTitle = "Flow vs Throughput";
  // std::string graphicsFileName        = fileNameWithNoExtension + ".png";
  // std::string plotFileName            = fileNameWithNoExtension + ".plt";
  // std::string plotTitle               = mainPlotTitle + ", Error: " + std::to_string(error);
  // std::string dataTitle               = "Throughput";

  // Instantiate the plot and set its title.
  // Gnuplot gnuplot (graphicsFileName);
  // gnuplot.SetTitle (plotTitle);

  // Make the graphics file, which the plot file will be when it
  // is used with Gnuplot, be a PNG file.
  // gnuplot.SetTerminal ("png");

  // Set the labels for each axis.
  // gnuplot.SetLegend ("Flow", "Throughput");


  // Gnuplot2dDataset dataset;
  // dataset.SetTitle (dataTitle);
  // dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  // Flow monitor.
  // Ptr<FlowMonitor> flowMonitor;
  // FlowMonitorHelper flowHelper;
  // flowMonitor = flowHelper.InstallAll ();

  // ThroughputMonitor (&flowHelper, flowMonitor, dataset);

  // Simulator::Stop (Seconds (duration));
  // Simulator::Run ();

  //Gnuplot ...continued.
  // gnuplot.AddDataset (dataset);
  // Open the plot file.
  // std::ofstream plotFile (plotFileName.c_str ());
  // Write the plot file.
  // gnuplot.GenerateOutput (plotFile);
  // Close the plot file.
  // plotFile.close ();
}