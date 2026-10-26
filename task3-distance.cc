/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/*
 * EECE5155 - Laboratory Assignment 1
 * Task 3: Throughput of a Wi-Fi Link versus Distance
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <iostream>
#include <sstream>

using namespace ns3;

int
main (int argc, char *argv[])
{
    // ------------------------------------------------------------
    // Reproducibility
    // ------------------------------------------------------------

    RngSeedManager::SetSeed (5155);
    RngSeedManager::SetRun (1);

    // Default distance between AP and STA
    double distance = 10.0;

    // Command-line argument
    CommandLine cmd (__FILE__);
    cmd.AddValue ("distance",
                  "Distance between AP and STA in meters",
                  distance);
    cmd.Parse (argc, argv);

    // ------------------------------------------------------------
    // Create nodes
    // ------------------------------------------------------------

    NodeContainer wifiStaNode;
    wifiStaNode.Create (1);

    NodeContainer wifiApNode;
    wifiApNode.Create (1);

    // ------------------------------------------------------------
    // Configure Wi-Fi
    // ------------------------------------------------------------

    WifiHelper wifi;

    // IEEE 802.11ac
    wifi.SetStandard (WIFI_STANDARD_80211ac);

    // Rate adaptation
    wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

    // ------------------------------------------------------------
    // Configure wireless channel
    // ------------------------------------------------------------

    YansWifiChannelHelper channel;

    channel.SetPropagationDelay (
        "ns3::ConstantSpeedPropagationDelayModel");

    channel.AddPropagationLoss (
        "ns3::FriisPropagationLossModel",
        "Frequency",
        DoubleValue (5.18e9));

    // ------------------------------------------------------------
    // Configure PHY
    // ------------------------------------------------------------

    YansWifiPhyHelper phy;

    phy.SetChannel (channel.Create ());

    // Fixed transmit power = 20 dBm
    phy.Set ("TxPowerStart", DoubleValue (20.0));
    phy.Set ("TxPowerEnd", DoubleValue (20.0));

    // Channel 36, 20 MHz, 5 GHz
    phy.Set (
        "ChannelSettings",
        StringValue ("{36, 20, BAND_5GHZ, 0}"));

    // Required for Wireshark
    phy.SetPcapDataLinkType (
        WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // ------------------------------------------------------------
    // Configure MAC layer
    // ------------------------------------------------------------

    WifiMacHelper mac;

    Ssid ssid = Ssid ("EECE5155");

    // STA
    mac.SetType (
        "ns3::StaWifiMac",
        "Ssid",
        SsidValue (ssid),
        "ActiveProbing",
        BooleanValue (false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install (
        phy,
        mac,
        wifiStaNode);

    // AP
    mac.SetType (
        "ns3::ApWifiMac",
        "Ssid",
        SsidValue (ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install (
        phy,
        mac,
        wifiApNode);

    // ------------------------------------------------------------
    // Mobility
    // ------------------------------------------------------------

    // AP position = (0, 0, 1.5)
    MobilityHelper apMobility;

    Ptr<ListPositionAllocator> apPosition =
        CreateObject<ListPositionAllocator> ();

    apPosition->Add (
        Vector (0.0, 0.0, 1.5));

    apMobility.SetPositionAllocator (
        apPosition);

    apMobility.SetMobilityModel (
        "ns3::ConstantPositionMobilityModel");

    apMobility.Install (
        wifiApNode);

    // STA position = (distance, 0, 1.5)
    MobilityHelper staMobility;

    Ptr<ListPositionAllocator> staPosition =
        CreateObject<ListPositionAllocator> ();

    staPosition->Add (
        Vector (distance, 0.0, 1.5));

    staMobility.SetPositionAllocator (
        staPosition);

    staMobility.SetMobilityModel (
        "ns3::ConstantPositionMobilityModel");

    staMobility.Install (
        wifiStaNode);

    // ------------------------------------------------------------
    // Internet stack
    // ------------------------------------------------------------

    InternetStackHelper stack;

    stack.Install (wifiStaNode);
    stack.Install (wifiApNode);

    // ------------------------------------------------------------
    // IP addresses
    // ------------------------------------------------------------

    Ipv4AddressHelper address;

    address.SetBase (
        "192.168.2.0",
        "255.255.255.0");

    Ipv4InterfaceContainer staInterface =
        address.Assign (staDevice);

    Ipv4InterfaceContainer apInterface =
        address.Assign (apDevice);

    // ------------------------------------------------------------
    // UDP Server on AP
    // ------------------------------------------------------------

    uint16_t port = 5003;

    UdpServerHelper server (port);

    ApplicationContainer serverApp =
        server.Install (wifiApNode.Get (0));

    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (13.0));

    // ------------------------------------------------------------
    // UDP Client on STA
    // ------------------------------------------------------------

    UdpClientHelper client (
        apInterface.GetAddress (0),
        port);

    client.SetAttribute (
        "MaxPackets",
        UintegerValue (300000));

    client.SetAttribute (
        "Interval",
        TimeValue (NanoSeconds (81920)));

    client.SetAttribute (
        "PacketSize",
        UintegerValue (1024));

    ApplicationContainer clientApp =
        client.Install (wifiStaNode.Get (0));

    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (12.0));

    // ------------------------------------------------------------
    // PCAP tracing
    // ------------------------------------------------------------

    std::ostringstream traceName;

    traceName
        << "task3-distance-"
        << distance
        << "m";

    phy.EnablePcap (
        traceName.str (),
        apDevice.Get (0));

    // ------------------------------------------------------------
    // FlowMonitor
    // ------------------------------------------------------------

    FlowMonitorHelper flowHelper;

    Ptr<FlowMonitor> monitor =
        flowHelper.InstallAll ();

    // Stop simulation at 13 seconds
    Simulator::Stop (Seconds (13.0));

    Simulator::Run ();

    // ------------------------------------------------------------
    // Throughput calculation
    // ------------------------------------------------------------

    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier> (
            flowHelper.GetClassifier ());

    for (const auto &flow :
         monitor->GetFlowStats ())
    {
        Ipv4FlowClassifier::FiveTuple t =
            classifier->FindFlow (
                flow.first);

        // Only STA -> AP UDP flow
        if (t.destinationPort != 5003 ||
            flow.second.rxPackets == 0)
        {
            continue;
        }

        double duration =
            (flow.second.timeLastRxPacket -
             flow.second.timeFirstRxPacket)
                .GetSeconds ();

        double throughput =
            flow.second.rxBytes *
            8.0 /
            duration /
            1e6;

        std::cout
            << "Distance : "
            << distance
            << " m"
            << std::endl;

        std::cout
            << "Rx packets : "
            << flow.second.rxPackets
            << std::endl;

        std::cout
            << "Throughput : "
            << throughput
            << " Mbps"
            << std::endl;
    }

    Simulator::Destroy ();

    return 0;
}
