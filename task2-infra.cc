/*
 * Lab 1, Task 2: Wi-Fi infrastructure mode (5 STAs + 1 AP)
 *
 * Run without RTS/CTS (questions 1-3):  ./ns3 run lab1-task2
 * Run with RTS/CTS forced (question 4):  ./ns3 run 'lab1-task2 --rtsCts=true'
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab1Task2");

int
main(int argc, char* argv[])
{
    bool verbose = true;
    uint32_t nWifi = 5;
    bool rtsCts = false; // default: do not force RTS/CTS

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("rtsCts", "Force the RTS/CTS handshake for every data frame", rtsCts);
    cmd.Parse(argc, argv);

    // Threshold 0: every frame larger than 0 bytes is preceded by RTS/CTS.
    // Must be set before the Wi-Fi devices are installed.
    if (rtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                           UintegerValue(0));
    }

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // 5 stations + 1 access point (the AP is a separate 6th node)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Default channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("EECE5155");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobility: STAs random walk in [-90,90]^2, AP fixed
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-90, 90, -90, 90)));
    mobility.Install(wifiStaNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Internet stack and addressing: one subnet 192.168.2.0/24
    InternetStackHelper stack;
    stack.SetIpv6StackInstall(false);
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    address.Assign(apDevices);

    // Echo server on STA Node 0, port 5002
    UdpEchoServerHelper echoServer(5002);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on Node 3: packets at 3 s and 5 s
    UdpEchoClientHelper client3(staInterfaces.GetAddress(0), 5002);
    client3.SetAttribute("MaxPackets", UintegerValue(2));
    client3.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    client3.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer c3 = client3.Install(wifiStaNodes.Get(3));
    c3.Start(Seconds(3.0));
    c3.Stop(Seconds(10.0));

    // Client on Node 4: packets at 2 s and 5 s
    UdpEchoClientHelper client4(staInterfaces.GetAddress(0), 5002);
    client4.SetAttribute("MaxPackets", UintegerValue(2));
    client4.SetAttribute("Interval", TimeValue(Seconds(3.0)));
    client4.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer c4 = client4.Install(wifiStaNodes.Get(4));
    c4.Start(Seconds(2.0));
    c4.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Beacons repeat forever, so the simulation must be stopped explicitly
    Simulator::Stop(Seconds(10.0));

    // Pcap on Node 4 (a client) and on the AP only; radiotap first.
    // The file name records which run produced it.
    std::string tag = rtsCts ? "-rtscts" : "-nortscts";
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("task2-node4" + tag, staDevices.Get(4));
    phy.EnablePcap("task2-ap" + tag, apDevices.Get(0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
