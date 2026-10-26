#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab1Task1");

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 5 nodes, all Wi-Fi
    NodeContainer wifiNodes;
    wifiNodes.Create(5);

    // Default channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // 802.11ac, ad-hoc MAC (no SSID, no AP)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // Random walk inside [-90,90] x [-90,90]
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
    mobility.Install(wifiNodes);

    // IPv4
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Echo server on Node 0, port 5001
    UdpEchoServerHelper echoServer(5001);
    ApplicationContainer serverApp = echoServer.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(10.0));

    // Client on Node 3: packets at 1s and 2s
    UdpEchoClientHelper client3(interfaces.GetAddress(0), 5001);
    client3.SetAttribute("MaxPackets", UintegerValue(2));
    client3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client3.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer c3 = client3.Install(wifiNodes.Get(3));
    c3.Start(Seconds(1.0));
    c3.Stop(Seconds(10.0));

    // Client on Node 4: packets at 2s and 4s
    UdpEchoClientHelper client4(interfaces.GetAddress(0), 5001);
    client4.SetAttribute("MaxPackets", UintegerValue(2));
    client4.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    client4.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer c4 = client4.Install(wifiNodes.Get(4));
    c4.Start(Seconds(2.0));
    c4.Stop(Seconds(10.0));

    // Pcap on Node 1 ONLY, radiotap header first
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("task1-node1", devices.Get(1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}