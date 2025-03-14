/*
 * Copyright (c) 2015 Magister Solutions
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Budiarto Herman <budiarto.herman@magister.fi>
 *
 */

#include "ns3/basic-data-calculators.h"
#include "ns3/config.h"
#include "ns3/error-model.h"
#include "ns3/integer.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/test.h"
#include "ns3/three-gpp-http-client.h"
#include "ns3/three-gpp-http-header.h"
#include "ns3/three-gpp-http-helper.h"
#include "ns3/three-gpp-http-server.h"

#include <list>
#include <optional>
#include <sstream>

NS_LOG_COMPONENT_DEFINE("ThreeGppHttpClientServerTest");

using namespace ns3;

// HTTP OBJECT TEST CASE //////////////////////////////////////////////////////

/**
 * @ingroup http
 * @ingroup applications-test
 * @ingroup tests
 * A test class which verifies that each HTTP object sent is also received the
 * same size.
 *
 * The test uses a minimalist scenario of one HTTP server and one HTTP client,
 * connected through a SimpleChannel. The simulation runs until 3 web pages
 * have been successfully downloaded by the client.
 *
 * The test also collects some statistical information from the simulation for
 * informational or debugging purpose. This can be seen by enabling LOG_INFO.
 */
class ThreeGppHttpObjectTestCase : public TestCase
{
  public:
    /**
     * @param name A textual label to briefly describe the test.
     * @param rngRun Run index to be used, intended to affect the values produced
     *               by random number generators throughout the test.
     * @param tcpType Type of TCP algorithm to be used by the connection between
     *                the client and the server. Must be a child type of
     *                ns3::TcpSocketFactory.
     * @param channelDelay Transmission delay between the client and the server
     *                     (and vice versa) which is due to the channel.
     * @param bitErrorRate The probability of transmission error between the
     *                     client and the server (and vice versa) in the unit of
     *                     bits.
     * @param mtuSize Maximum transmission unit (in bytes) to be used by the
     *                server model.
     * @param useIpv6 If true, IPv6 will be used to address both client and
     *                server. Otherwise, IPv4 will be used.
     * @param port The port to use if provided, otherwise the default port is used.
     */
    ThreeGppHttpObjectTestCase(const std::string& name,
                               uint32_t rngRun,
                               const TypeId& tcpType,
                               const Time& channelDelay,
                               double bitErrorRate,
                               uint32_t mtuSize,
                               bool useIpv6,
                               std::optional<uint16_t> port);

  private:
    /**
     * Creates a Node, complete with a TCP/IP stack.
     * #m_tcpType determines the TCP algorithm installed at the TCP stack.
     *
     * \param[in] channel Pointer to a channel which the node's device will be
     *                    attached to.
     * @return Pointer to the newly created node.
     */
    Ptr<Node> CreateSimpleInternetNode(Ptr<SimpleChannel> channel);

    /**
     * Assign a socket address for a device.
     * #m_useIpv6 determines whether to use IPv4 addressing or IPv6 addressing.
     *
     * \param[in] dev Pointer to the device.
     * \param[in] port the port to use for the socket address.
     * @return The resulting socket address.
     */
    Address AssignSocketAddress(Ptr<NetDevice> dev, uint16_t port);

    /**
     * Assign an IPv4 address to a device.
     *
     * \param[in] dev Pointer to the device to assign an address to.
     * \param[in] logging flag to indicate whether to log the assigned address.
     * @return The resulting IPv4 address of the device.
     */
    Ipv4Address AssignIpv4Address(Ptr<NetDevice> dev, bool logging = true);

    /**
     * Assign an IPv6 address to a device.
     *
     * \param[in] dev Pointer to the device to assign an address to.
     * \param[in] logging flag to indicate whether to log the assigned address.
     * @return The resulting IPv6 address of the device.
     */
    Ipv6Address AssignIpv6Address(Ptr<NetDevice> dev, bool logging = true);

    // Inherited from TestCase base class.
    void DoRun() override;
    void DoTeardown() override;

    /**
     * @internal
     * Internal class used by ThreeGppHttpObjectTestCase. Keep track of the number
     * of object and bytes that have been sent and received in the simulation by
     * listening to the relevant trace sources.
     */
    class ThreeGppHttpObjectTracker
    {
      public:
        /// Creates a new instance with all counters begin at zero.
        ThreeGppHttpObjectTracker();
        /**
         * Shall be invoked when a whole object has been transmitted.
         * @param size Size of the whole object (in bytes).
         */
        void ObjectSent(uint32_t size);
        /**
         * Shall be invoked when an object part has been received.
         * @param size Size of the object part (in bytes). This amount will be
         *             accumulated until ObjectReceived() is invoked.
         */
        void PartReceived(uint32_t size);
        /**
         * Shall be invoked after all parts of a complete object have been
         * received.
         * \param[out] txSize Size of the whole object (in bytes) when it was
         *                    transmitted.
         * \param[out] rxSize Size of the whole object (in bytes) received.
         * @return True if this receive operation has a matching transmission
         *         operation (ObjectSent()), otherwise false. Both arguments are
         *         guaranteed to be replaced with initialized values if the return
         *         value is true.
         */
        bool ObjectReceived(uint32_t& txSize, uint32_t& rxSize);
        /// @return True if zero object is currently tracked.
        bool IsEmpty() const;
        /// @return Number of whole objects that have been received so far.
        uint16_t GetNumOfObjectsReceived() const;

      private:
        /**
         * Each entry is the size (in bytes) of object transmitted. A new entry is
         * pushed to the back when a new object is transmitted. The frontmost entry
         * is then removed when a whole object is received, i.e., it's logically a
         * first-in-first-out queue data structure.
         */
        std::list<uint32_t> m_objectsSize;
        /// The accumulated size (in bytes) of parts of a whole object.
        uint32_t m_rxBuffer;
        /// Number of whole objects that have been received so far.
        uint16_t m_numOfObjectsReceived;
    };

    // The following defines one tracker for each HTTP object type.
    ThreeGppHttpObjectTracker m_requestObjectTracker;  ///< Tracker of request objects.
    ThreeGppHttpObjectTracker m_mainObjectTracker;     ///< Tracker of main objects.
    ThreeGppHttpObjectTracker m_embeddedObjectTracker; ///< Tracker of embedded objects.

    // CALLBACK TO TRACE SOURCES.

    /**
     * Connected with `TxMainObjectRequest` trace source of the client.
     * Updates #m_requestObjectTracker.
     * @param packet The packet of main object sent.
     */
    void ClientTxMainObjectRequestCallback(Ptr<const Packet> packet);
    /**
     * Connected with `TxEmbeddedObjectRequest` trace source of the client.
     * Updates #m_requestObjectTracker.
     * @param packet The packet of embedded object sent.
     */
    void ClientTxEmbeddedObjectRequestCallback(Ptr<const Packet> packet);
    /**
     * Connected with `Rx` trace source of the server.
     * Updates #m_requestObjectTracker and perform some tests on the packet and
     * the size of the object.
     * @param packet The packet received.
     * @param from The address where the packet originates from.
     * @param to The local address on which the server binds to.
     */
    void ServerRxCallback(Ptr<const Packet> packet, const Address& from, const Address& to);
    /**
     * Connected with `MainObject` trace source of the server.
     * Updates #m_mainObjectTracker.
     * @param size Size of the generated main object (in bytes).
     */
    void ServerMainObjectCallback(uint32_t size);
    /**
     * Connected with `RxMainObjectPacket` trace source of the client.
     * Updates #m_mainObjectTracker and perform some tests on the packet.
     * @param packet The packet received.
     */
    void ClientRxMainObjectPacketCallback(Ptr<const Packet> packet);
    /**
     * Connected with `RxMainObject` trace source of the client. Updates
     * #m_mainObjectTracker and perform some tests on the size of the object.
     * @param httpClient Pointer to the application.
     * @param packet Full packet received by application.
     */
    void ClientRxMainObjectCallback(Ptr<const ThreeGppHttpClient> httpClient,
                                    Ptr<const Packet> packet);
    /**
     * Connected with `EmbeddedObject` trace source of the server.
     * Updates #m_embeddedObjectTracker.
     * @param size Size of the generated embedded object (in bytes).
     */
    void ServerEmbeddedObjectCallback(uint32_t size);
    /**
     * Connected with `RxEmbeddedObjectPacket` trace source of the client.
     * Updates #m_embeddedObjectTracker and perform some tests on the packet.
     * @param packet The packet received.
     */
    void ClientRxEmbeddedObjectPacketCallback(Ptr<const Packet> packet);
    /**
     * Connected with `RxEmbeddedObject` trace source of the client. Updates
     * #m_embeddedObjectTracker and perform some tests on the size of the object.
     * @param httpClient Pointer to the application.
     * @param packet Full packet received by application.
     */
    void ClientRxEmbeddedObjectCallback(Ptr<const ThreeGppHttpClient> httpClient,
                                        Ptr<const Packet> packet);
    /**
     * Connected with `StateTransition` trace source of the client.
     * Increments #m_numOfPagesReceived when the client enters READING state.
     * @param oldState The name of the previous state.
     * @param newState The name of the current state.
     */
    void ClientStateTransitionCallback(const std::string& oldState, const std::string& newState);
    /**
     * Connected with `RxDelay` trace source of the client.
     * Updates the statistics in #m_delayCalculator.
     * @param delay The packet one-trip delay time.
     * @param from The address of the device where the packet originates from.
     */
    void ClientRxDelayCallback(const Time& delay, const Address& from);
    /**
     * Connected with `RxRtt` trace source of the client.
     * Updates the statistics in #m_rttCalculator.
     * @param rtt The packet round trip delay time.
     * @param from The address of the device where the packet originates from.
     */
    void ClientRxRttCallback(const Time& rtt, const Address& from);
    /**
     * Connected with `PhyRxDrop` trace source of both the client's and server's
     * devices. Increments #m_numOfPacketDrops.
     * @param packet Pointer to the packet being dropped.
     */
    void DeviceDropCallback(Ptr<const Packet> packet);
    /**
     * Dummy event
     */
    void ProgressCallback();

    // THE PARAMETERS OF THE TEST CASE.

    uint32_t m_rngRun;              ///< Determines the set of random values generated.
    TypeId m_tcpType;               ///< TCP algorithm used.
    Time m_channelDelay;            ///< %Time needed by a packet to propagate.
    uint32_t m_mtuSize;             ///< Maximum transmission unit (in bytes).
    bool m_useIpv6;                 ///< Whether to use IPv6 or IPv4.
    std::optional<uint16_t> m_port; ///< port to use if provided, otherwise the default port is used

    // OTHER MEMBER VARIABLES.

    /// Receive error model to be attached to the devices of both directions.
    Ptr<RateErrorModel> m_errorModel;
    /// Begins with 0. Simulation stops if this reaches 3.
    uint16_t m_numOfPagesReceived;
    /// Number of packets dropped because of #m_errorModel.
    uint16_t m_numOfPacketDrops;
    /// Installs TCP/IP stack on the nodes.
    InternetStackHelper m_internetStackHelper;
    /// Assigns IPv4 addresses to the nodes.
    Ipv4AddressHelper m_ipv4AddressHelper;
    /// Assigns IPv6 addresses to the nodes.
    Ipv6AddressHelper m_ipv6AddressHelper;
    /// Keeps statistical information of one-trip delays (in seconds).
    Ptr<MinMaxAvgTotalCalculator<double>> m_delayCalculator;
    /// Keeps statistical information of round-trip delays (in seconds).
    Ptr<MinMaxAvgTotalCalculator<double>> m_rttCalculator;

}; // end of `class HttpClientServerTestCase`

ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTestCase(const std::string& name,
                                                       uint32_t rngRun,
                                                       const TypeId& tcpType,
                                                       const Time& channelDelay,
                                                       double bitErrorRate,
                                                       uint32_t mtuSize,
                                                       bool useIpv6,
                                                       std::optional<uint16_t> port)
    : TestCase(name),
      m_rngRun{rngRun},
      m_tcpType{tcpType},
      m_channelDelay{channelDelay},
      m_mtuSize{mtuSize},
      m_useIpv6{useIpv6},
      m_port{port},
      m_numOfPagesReceived{0},
      m_numOfPacketDrops{0}
{
    NS_LOG_FUNCTION(this << GetName());

    // NS_ASSERT (tcpType.IsChildOf (TypeId::LookupByName ("ns3::TcpSocketBase")));
    NS_ASSERT(channelDelay.IsPositive());

    m_errorModel = CreateObject<RateErrorModel>();
    m_errorModel->SetRate(bitErrorRate);
    m_errorModel->SetUnit(RateErrorModel::ERROR_UNIT_BIT);

    m_ipv4AddressHelper.SetBase(Ipv4Address("10.0.0.0"),
                                Ipv4Mask("255.0.0.0"),
                                Ipv4Address("0.0.0.1"));
    m_ipv6AddressHelper.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64), Ipv6Address("::1"));

    m_delayCalculator = CreateObject<MinMaxAvgTotalCalculator<double>>();
    m_rttCalculator = CreateObject<MinMaxAvgTotalCalculator<double>>();
}

Ptr<Node>
ThreeGppHttpObjectTestCase::CreateSimpleInternetNode(Ptr<SimpleChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);

    Ptr<SimpleNetDevice> dev = CreateObject<SimpleNetDevice>();
    dev->SetAddress(Mac48Address::Allocate());
    dev->SetChannel(channel);
    dev->SetReceiveErrorModel(m_errorModel);

    Ptr<Node> node = CreateObject<Node>();
    node->AddDevice(dev);
    m_internetStackHelper.Install(node);

    // Set the TCP algorithm.
    Ptr<TcpL4Protocol> tcp = node->GetObject<TcpL4Protocol>();
    tcp->SetAttribute("SocketType", TypeIdValue(m_tcpType));

    // Connect with the trace source that informs about packet drop due to error.
    dev->TraceConnectWithoutContext(
        "PhyRxDrop",
        MakeCallback(&ThreeGppHttpObjectTestCase::DeviceDropCallback, this));

    return node;
}

Ipv4Address
ThreeGppHttpObjectTestCase::AssignIpv4Address(Ptr<NetDevice> dev, bool logging)
{
    NS_LOG_FUNCTION(this);
    Ipv4InterfaceContainer ipv4Ifs = m_ipv4AddressHelper.Assign(NetDeviceContainer(dev));
    NS_ASSERT(ipv4Ifs.GetN() == 1);
    const auto assignedAddress = ipv4Ifs.GetAddress(0, 0);
    if (logging)
    {
        NS_LOG_DEBUG(this << " node is assigned to " << assignedAddress << ".");
    }
    return assignedAddress;
}

Ipv6Address
ThreeGppHttpObjectTestCase::AssignIpv6Address(Ptr<NetDevice> dev, bool logging)
{
    NS_LOG_FUNCTION(this);
    Ipv6InterfaceContainer ipv6Ifs = m_ipv6AddressHelper.Assign(NetDeviceContainer(dev));
    NS_ASSERT(ipv6Ifs.GetN() == 1);
    const auto assignedAddress = ipv6Ifs.GetAddress(0, 0);
    if (logging)
    {
        NS_LOG_DEBUG(this << " node is assigned to " << assignedAddress << ".");
    }
    return assignedAddress;
}

Address
ThreeGppHttpObjectTestCase::AssignSocketAddress(Ptr<NetDevice> dev, uint16_t port)
{
    NS_LOG_FUNCTION(this);
    Address assignedAddress;

    // Assign IP address according to the selected Ip version.
    if (m_useIpv6)
    {
        assignedAddress = Inet6SocketAddress(AssignIpv6Address(dev, false), port);
    }
    else
    {
        assignedAddress = InetSocketAddress(AssignIpv4Address(dev, false), port);
    }

    NS_LOG_DEBUG(this << " node is assigned to " << assignedAddress << ".");

    return assignedAddress;
}

void
ThreeGppHttpObjectTestCase::DoRun()
{
    NS_LOG_FUNCTION(this << GetName());
    Config::SetGlobal("RngRun", UintegerValue(m_rngRun));
    NS_LOG_INFO(this << " Running test case " << GetName());

    /*
     * Create topology:
     *
     *     Server Node                  Client Node
     * +-----------------+          +-----------------+
     * |   HTTP Server   |          |   HTTP Client   |
     * |   Application   |          |   Application   |
     * +-----------------+          +-----------------+
     * |       TCP       |          |       TCP       |
     * +-----------------+          +-----------------+
     * |     IPv4/v6     |          |     IPv4/v6     |
     * +-----------------+          +-----------------+
     * |  Simple NetDev  |          |  Simple NetDev  |
     * +-----------------+          +-----------------+
     *          |                            |
     *          |                            |
     *          +----------------------------+
     *                  Simple Channel
     */

    // Channel.
    auto channel = CreateObject<SimpleChannel>();
    channel->SetAttribute("Delay", TimeValue(m_channelDelay));

    // Server and client nodes.
    ApplicationContainer serverApplications{};
    ApplicationContainer clientApplications{};
    auto serverNode = CreateSimpleInternetNode(channel);
    auto clientNode = CreateSimpleInternetNode(channel);

    // applications.
    if (m_port)
    {
        const auto serverAddress = AssignSocketAddress(serverNode->GetDevice(0), *m_port);
        ThreeGppHttpServerHelper serverHelper(serverAddress);
        serverApplications = serverHelper.Install(serverNode);
        AssignSocketAddress(clientNode->GetDevice(0), *m_port);
        ThreeGppHttpClientHelper clientHelper(serverAddress);
        clientApplications = clientHelper.Install(clientNode);
    }
    else
    {
        if (m_useIpv6)
        {
            const auto serverAddress = AssignIpv6Address(serverNode->GetDevice(0));
            ThreeGppHttpServerHelper serverHelper(serverAddress);
            serverApplications = serverHelper.Install(serverNode);
            AssignIpv6Address(clientNode->GetDevice(0));
            ThreeGppHttpClientHelper clientHelper(serverAddress);
            clientApplications = clientHelper.Install(clientNode);
        }
        else
        {
            const auto serverAddress = AssignIpv4Address(serverNode->GetDevice(0));
            ThreeGppHttpServerHelper serverHelper(serverAddress);
            serverApplications = serverHelper.Install(serverNode);
            AssignIpv4Address(clientNode->GetDevice(0));
            ThreeGppHttpClientHelper clientHelper(serverAddress);
            clientApplications = clientHelper.Install(clientNode);
        }
    }
    NS_TEST_ASSERT_MSG_EQ(serverApplications.GetN(),
                          1,
                          "Invalid number of HTTP servers has been installed");
    auto httpServer = serverApplications.Get(0)->GetObject<ThreeGppHttpServer>();
    NS_TEST_ASSERT_MSG_NE(httpServer,
                          nullptr,
                          "HTTP server installation fails to produce a proper type");
    httpServer->SetMtuSize(m_mtuSize);
    NS_TEST_ASSERT_MSG_EQ(clientApplications.GetN(),
                          1,
                          "Invalid number of HTTP clients has been installed");
    auto httpClient = clientApplications.Get(0)->GetObject<ThreeGppHttpClient>();
    NS_TEST_ASSERT_MSG_NE(httpClient,
                          nullptr,
                          "HTTP client installation fails to produce a proper type");

    // Uplink (requests) trace sources.
    bool traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "TxMainObjectRequest",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientTxMainObjectRequestCallback, this));
    NS_ASSERT(traceSourceConnected);
    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "TxEmbeddedObjectRequest",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientTxEmbeddedObjectRequestCallback, this));
    NS_ASSERT(traceSourceConnected);
    traceSourceConnected = httpServer->TraceConnectWithoutContext(
        "RxWithAddresses",
        MakeCallback(&ThreeGppHttpObjectTestCase::ServerRxCallback, this));
    NS_ASSERT(traceSourceConnected);

    // Downlink (main objects) trace sources.
    traceSourceConnected = httpServer->TraceConnectWithoutContext(
        "MainObject",
        MakeCallback(&ThreeGppHttpObjectTestCase::ServerMainObjectCallback, this));
    NS_ASSERT(traceSourceConnected);
    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "RxMainObjectPacket",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientRxMainObjectPacketCallback, this));
    NS_ASSERT(traceSourceConnected);
    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "RxMainObject",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientRxMainObjectCallback, this));
    NS_ASSERT(traceSourceConnected);

    // Downlink (embedded objects) trace sources.
    traceSourceConnected = httpServer->TraceConnectWithoutContext(
        "EmbeddedObject",
        MakeCallback(&ThreeGppHttpObjectTestCase::ServerEmbeddedObjectCallback, this));
    NS_ASSERT(traceSourceConnected);

    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "RxEmbeddedObjectPacket",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientRxEmbeddedObjectPacketCallback, this));
    NS_ASSERT(traceSourceConnected);

    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "RxEmbeddedObject",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientRxEmbeddedObjectCallback, this));
    NS_ASSERT(traceSourceConnected);

    // Other trace sources.
    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "StateTransition",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientStateTransitionCallback, this));
    NS_ASSERT(traceSourceConnected);
    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "RxDelay",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientRxDelayCallback, this));
    NS_ASSERT(traceSourceConnected);
    traceSourceConnected = httpClient->TraceConnectWithoutContext(
        "RxRtt",
        MakeCallback(&ThreeGppHttpObjectTestCase::ClientRxRttCallback, this));
    NS_ASSERT(traceSourceConnected);

    Simulator::Schedule(Seconds(1), &ThreeGppHttpObjectTestCase::ProgressCallback, this);

    /*
     * Here we don't set the simulation stop time. During the run, the simulation
     * will stop immediately after the client has completely received the third
     * web page.
     */
    Simulator::Run();

    // Dump some statistical information about the simulation.
    NS_LOG_INFO(this << " Total request objects received: "
                     << m_requestObjectTracker.GetNumOfObjectsReceived() << " object(s).");
    NS_LOG_INFO(this << " Total main objects received: "
                     << m_mainObjectTracker.GetNumOfObjectsReceived() << " object(s).");
    NS_LOG_INFO(this << " Total embedded objects received: "
                     << m_embeddedObjectTracker.GetNumOfObjectsReceived() << " object(s).");
    NS_LOG_INFO(this << " One-trip delays: average=" << m_delayCalculator->getMean() << " min="
                     << m_delayCalculator->getMin() << " max=" << m_delayCalculator->getMax());
    NS_LOG_INFO(this << " Round-trip delays: average=" << m_rttCalculator->getMean() << " min="
                     << m_rttCalculator->getMin() << " max=" << m_rttCalculator->getMax());
    NS_LOG_INFO(this << " Number of packets dropped by the devices: " << m_numOfPacketDrops
                     << " packet(s).");

    // Some post-simulation tests.
    NS_TEST_EXPECT_MSG_EQ(m_numOfPagesReceived, 3, "Unexpected number of web pages processed.");
    NS_TEST_EXPECT_MSG_EQ(m_requestObjectTracker.IsEmpty(),
                          true,
                          "Tracker of request objects detected irrelevant packet(s).");
    NS_TEST_EXPECT_MSG_EQ(m_mainObjectTracker.IsEmpty(),
                          true,
                          "Tracker of main objects detected irrelevant packet(s).");
    NS_TEST_EXPECT_MSG_EQ(m_embeddedObjectTracker.IsEmpty(),
                          true,
                          "Tracker of embedded objects detected irrelevant packet(s).");

    Simulator::Destroy();
}

void
ThreeGppHttpObjectTestCase::DoTeardown()
{
    NS_LOG_FUNCTION(this << GetName());
}

ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTracker::ThreeGppHttpObjectTracker()
    : m_rxBuffer(0),
      m_numOfObjectsReceived(0)
{
    NS_LOG_FUNCTION(this);
}

void
ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTracker::ObjectSent(uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_objectsSize.push_back(size);
}

void
ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTracker::PartReceived(uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_rxBuffer += size;
}

bool
ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTracker::ObjectReceived(uint32_t& txSize,
                                                                      uint32_t& rxSize)
{
    NS_LOG_FUNCTION(this);

    if (m_objectsSize.empty())
    {
        return false;
    }

    // Set output values.
    txSize = m_objectsSize.front();
    rxSize = m_rxBuffer;

    // Reset counters.
    m_objectsSize.pop_front();
    m_rxBuffer = 0;
    m_numOfObjectsReceived++;

    return true;
}

bool
ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTracker::IsEmpty() const
{
    return (m_objectsSize.empty() && (m_rxBuffer == 0));
}

uint16_t
ThreeGppHttpObjectTestCase::ThreeGppHttpObjectTracker::GetNumOfObjectsReceived() const
{
    return m_numOfObjectsReceived;
}

void
ThreeGppHttpObjectTestCase::ClientTxMainObjectRequestCallback(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet << packet->GetSize());
    m_requestObjectTracker.ObjectSent(packet->GetSize());
}

void
ThreeGppHttpObjectTestCase::ClientTxEmbeddedObjectRequestCallback(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet << packet->GetSize());
    m_requestObjectTracker.ObjectSent(packet->GetSize());
}

void
ThreeGppHttpObjectTestCase::ServerRxCallback(Ptr<const Packet> packet,
                                             const Address& from,
                                             const Address& to)
{
    NS_LOG_INFO(this << packet << packet->GetSize() << from << to);

    uint16_t port{};
    if (InetSocketAddress::IsMatchingType(to))
    {
        port = InetSocketAddress::ConvertFrom(to).GetPort();
    }
    else if (Inet6SocketAddress::IsMatchingType(to))
    {
        port = Inet6SocketAddress::ConvertFrom(to).GetPort();
    }
    NS_TEST_ASSERT_MSG_EQ(port,
                          m_port.value_or(ThreeGppHttpServer::HTTP_DEFAULT_PORT),
                          "Incorrect port");

    // Check the header in packet
    Ptr<Packet> copy = packet->Copy();
    ThreeGppHttpHeader httpHeader;
    NS_TEST_ASSERT_MSG_EQ(copy->RemoveHeader(httpHeader),
                          22,
                          "Error finding ThreeGppHttpHeader in a packet received by the server");
    NS_TEST_ASSERT_MSG_GT(httpHeader.GetClientTs(),
                          Seconds(0),
                          "Request object's client TS is unexpectedly non-positive");

    m_requestObjectTracker.PartReceived(packet->GetSize());

    /*
     * Request objects are assumed to be small and to not typically split. So we
     * immediately follow by concluding the receive of a whole request object.
     */
    uint32_t txSize = 0;
    uint32_t rxSize = 0;
    bool isSent = m_requestObjectTracker.ObjectReceived(txSize, rxSize);
    NS_TEST_ASSERT_MSG_EQ(isSent, true, "Server receives one too many request object");
    NS_TEST_ASSERT_MSG_EQ(txSize,
                          rxSize,
                          "Transmitted size and received size of request object differ");
}

void
ThreeGppHttpObjectTestCase::ServerMainObjectCallback(uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_mainObjectTracker.ObjectSent(size);
}

void
ThreeGppHttpObjectTestCase::ClientRxMainObjectPacketCallback(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet << packet->GetSize());
    m_mainObjectTracker.PartReceived(packet->GetSize());
}

void
ThreeGppHttpObjectTestCase::ClientRxMainObjectCallback(Ptr<const ThreeGppHttpClient> httpClient,
                                                       Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << httpClient << httpClient->GetNode()->GetId());

    // Verify the header in the packet.
    Ptr<Packet> copy = packet->Copy();
    ThreeGppHttpHeader httpHeader;
    NS_TEST_ASSERT_MSG_EQ(copy->RemoveHeader(httpHeader),
                          22,
                          "Error finding ThreeGppHttpHeader in a packet received by the server");
    NS_TEST_ASSERT_MSG_EQ(httpHeader.GetContentType(),
                          ThreeGppHttpHeader::MAIN_OBJECT,
                          "Invalid content type in the received packet");
    NS_TEST_ASSERT_MSG_GT(httpHeader.GetClientTs(),
                          Seconds(0),
                          "Main object's client TS is unexpectedly non-positive");
    NS_TEST_ASSERT_MSG_GT(httpHeader.GetServerTs(),
                          Seconds(0),
                          "Main object's server TS is unexpectedly non-positive");

    uint32_t txSize = 0;
    uint32_t rxSize = 0;
    bool isSent = m_mainObjectTracker.ObjectReceived(txSize, rxSize);
    NS_TEST_ASSERT_MSG_EQ(isSent, true, "Client receives one too many main object");
    NS_TEST_ASSERT_MSG_EQ(txSize,
                          rxSize,
                          "Transmitted size and received size of main object differ");
    NS_TEST_ASSERT_MSG_EQ(httpHeader.GetContentLength(),
                          rxSize,
                          "Actual main object packet size and received size of main object differ");
}

void
ThreeGppHttpObjectTestCase::ServerEmbeddedObjectCallback(uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_embeddedObjectTracker.ObjectSent(size);
}

void
ThreeGppHttpObjectTestCase::ClientRxEmbeddedObjectPacketCallback(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet << packet->GetSize());
    m_embeddedObjectTracker.PartReceived(packet->GetSize());
}

void
ThreeGppHttpObjectTestCase::ClientRxEmbeddedObjectCallback(Ptr<const ThreeGppHttpClient> httpClient,
                                                           Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << httpClient << httpClient->GetNode()->GetId());

    // Verify the header in the packet.
    Ptr<Packet> copy = packet->Copy();
    ThreeGppHttpHeader httpHeader;
    NS_TEST_ASSERT_MSG_EQ(copy->RemoveHeader(httpHeader),
                          22,
                          "Error finding ThreeGppHttpHeader in a packet received by the server");
    NS_TEST_ASSERT_MSG_EQ(httpHeader.GetContentType(),
                          ThreeGppHttpHeader::EMBEDDED_OBJECT,
                          "Invalid content type in the received packet");
    NS_TEST_ASSERT_MSG_GT(httpHeader.GetClientTs(),
                          Seconds(0),
                          "Embedded object's client TS is unexpectedly non-positive");
    NS_TEST_ASSERT_MSG_GT(httpHeader.GetServerTs(),
                          Seconds(0),
                          "Embedded object's server TS is unexpectedly non-positive");

    uint32_t txSize = 0;
    uint32_t rxSize = 0;
    bool isSent = m_embeddedObjectTracker.ObjectReceived(txSize, rxSize);
    NS_TEST_ASSERT_MSG_EQ(isSent, true, "Client receives one too many embedded object");
    NS_TEST_ASSERT_MSG_EQ(txSize,
                          rxSize,
                          "Transmitted size and received size of embedded object differ");
    NS_TEST_ASSERT_MSG_EQ(
        httpHeader.GetContentLength(),
        rxSize,
        "Actual embedded object packet size and received size of embedded object differ");
}

void
ThreeGppHttpObjectTestCase::ClientStateTransitionCallback(const std::string& oldState,
                                                          const std::string& newState)
{
    NS_LOG_FUNCTION(this << oldState << newState);

    if (newState == "READING")
    {
        m_numOfPagesReceived++;

        if (m_numOfPagesReceived >= 3)
        {
            // We have processed 3 web pages and that should be enough for this test.
            NS_LOG_LOGIC(this << " Test is stopping now.");
            Simulator::Stop();
        }
    }
}

void
ThreeGppHttpObjectTestCase::ProgressCallback()
{
    NS_LOG_DEBUG("Simulator time now: " << Simulator::Now().As(Time::S) << ".");
    Simulator::Schedule(Seconds(1), &ThreeGppHttpObjectTestCase::ProgressCallback, this);
}

void
ThreeGppHttpObjectTestCase::ClientRxDelayCallback(const Time& delay, const Address& from)
{
    NS_LOG_FUNCTION(this << delay.As(Time::S) << from);
    m_delayCalculator->Update(delay.GetSeconds());
}

void
ThreeGppHttpObjectTestCase::ClientRxRttCallback(const Time& rtt, const Address& from)
{
    NS_LOG_FUNCTION(this << rtt.As(Time::S) << from);
    m_rttCalculator->Update(rtt.GetSeconds());
}

void
ThreeGppHttpObjectTestCase::DeviceDropCallback(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet << packet->GetSize());
    m_numOfPacketDrops++;
}

// TEST SUITE /////////////////////////////////////////////////////////////////

/**
 * @ingroup http
 * @ingroup applications-test
 * @ingroup tests
 * A test class for running several system tests which validate the web
 * browsing traffic model.
 *
 * The tests cover the combinations of the following parameters:
 *   - the use of NewReno (ns-3's default)
 *   - various lengths of channel delay: 3 ms, 30 ms, and 300 ms;
 *   - the existence of transmission error;
 *   - different MTU (maximum transmission unit) sizes;
 *   - IPv4 and IPv6; and
 *   - the use of different set of random numbers.
 *
 * The _fullness_ parameter specified when running the test framework will
 * determine the number of test cases created by this test suite.
 */
class ThreeGppHttpClientServerTestSuite : public TestSuite
{
  public:
    /// Instantiate the test suite.
    ThreeGppHttpClientServerTestSuite()
        : TestSuite("applications-three-gpp-http-client-server", Type::SYSTEM)
    {
        // LogComponentEnable ("ThreeGppHttpClientServerTest", LOG_INFO);
        // LogComponentEnable ("ThreeGppHttpClient", LOG_INFO);
        // LogComponentEnable ("ThreeGppHttpServer", LOG_INFO);
        // LogComponentEnableAll (LOG_PREFIX_ALL);

        Time channelDelay[] = {MilliSeconds(3), MilliSeconds(30), MilliSeconds(300)};
        double bitErrorRate[] = {0.0, 5.0e-6};
        uint32_t mtuSize[] = {536, 1460};

        uint32_t run = 1;
        while (run <= 100)
        {
            for (uint32_t i1 = 0; i1 < 3; i1++)
            {
                for (uint32_t i2 = 0; i2 < 2; i2++)
                {
                    for (uint32_t i3 = 0; i3 < 2; i3++)
                    {
                        AddHttpObjectTestCase(run++,
                                              channelDelay[i1],
                                              bitErrorRate[i2],
                                              mtuSize[i3],
                                              false);
                        AddHttpObjectTestCase(run++,
                                              channelDelay[i1],
                                              bitErrorRate[i2],
                                              mtuSize[i3],
                                              false,
                                              8080);
                        AddHttpObjectTestCase(run++,
                                              channelDelay[i1],
                                              bitErrorRate[i2],
                                              mtuSize[i3],
                                              true);
                    }
                }
            }
        }
    }

  private:
    /**
     * Creates a test case with the given parameters.
     *
     * @param rngRun Run index to be used, intended to affect the values produced
     *               by random number generators throughout the test.
     * @param channelDelay Transmission delay between the client and the server
     *                     (and vice versa) which is due to the channel.
     * @param bitErrorRate The probability of transmission error between the
     *                     client and the server (and vice versa) in the unit of
     *                     bits.
     * @param mtuSize Maximum transmission unit (in bytes) to be used by the
     *                server model.
     * @param useIpv6 If true, IPv6 will be used to address both client and
     *                server. Otherwise, IPv4 will be used.
     * @param port The port to use if provided, otherwise the default port is used.
     */
    void AddHttpObjectTestCase(uint32_t rngRun,
                               const Time& channelDelay,
                               double bitErrorRate,
                               uint32_t mtuSize,
                               bool useIpv6,
                               std::optional<uint16_t> port = std::nullopt)
    {
        std::ostringstream name;
        name << "Run #" << rngRun;
        name << " delay=" << channelDelay.As(Time::MS);
        name << " ber=" << bitErrorRate;
        name << " mtu=" << mtuSize;

        if (useIpv6)
        {
            name << " IPv6";
        }
        else
        {
            name << " IPv4";
        }
        if (port)
        {
            name << "(" << *port << ")";
        }

        // Assign higher fullness for tests with higher RngRun.
        TestCase::Duration testDuration = TestCase::Duration::QUICK;
        if (rngRun > 20)
        {
            testDuration = TestCase::Duration::EXTENSIVE;
        }
        if (rngRun > 50)
        {
            testDuration = TestCase::Duration::TAKES_FOREVER;
        }

        AddTestCase(new ThreeGppHttpObjectTestCase(name.str(),
                                                   rngRun,
                                                   TcpNewReno::GetTypeId(),
                                                   channelDelay,
                                                   bitErrorRate,
                                                   mtuSize,
                                                   useIpv6,
                                                   port),
                    testDuration);
    }

}; // end of class `ThreeGppHttpClientServerTestSuite`

/// The global instance of the `three-gpp-http-client-server` system test.
static ThreeGppHttpClientServerTestSuite g_httpClientServerTestSuiteInstance;
