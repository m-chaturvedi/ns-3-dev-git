/*
 * Copyright (c) 2011, 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include "ns3/building-position-allocator.h"
#include "ns3/building.h"
#include "ns3/buildings-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/log.h"
#include "ns3/mobility-building-info.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BuildingPositionAllocatorTest");

/**
 * @ingroup buildings
 * @ingroup tests
 * @defgroup building-test Buildings module tests
 */

/**
 * @ingroup building-test
 *
 * Room coordinates
 */
struct Room
{
    /**
     * Constructor
     * @param xx X coord
     * @param yy Y coord
     * @param zz Z coord
     */
    Room(uint32_t xx, uint32_t yy, uint32_t zz);
    uint32_t x; //!< X coord
    uint32_t y; //!< Y coord
    uint32_t z; //!< Z coord (floor)
};

Room::Room(uint32_t xx, uint32_t yy, uint32_t zz)
    : x(xx),
      y(yy),
      z(zz)
{
}

bool
operator<(const Room& a, const Room& b)
{
    return ((a.x < b.x) || ((a.x == b.x) && (a.y < b.y)) ||
            ((a.x == b.x) && (a.y == b.y) && (a.z < b.z)));
}

/**
 * @ingroup building-test
 *
 * RandomRoomPositionAllocator test
 */
class RandomRoomPositionAllocatorTestCase : public TestCase
{
  public:
    RandomRoomPositionAllocatorTestCase();

  private:
    void DoRun() override;
};

RandomRoomPositionAllocatorTestCase::RandomRoomPositionAllocatorTestCase()
    : TestCase("RandomRoom, 12 rooms, 24 nodes")
{
}

void
RandomRoomPositionAllocatorTestCase::DoRun()
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("create building");
    Ptr<Building> b = CreateObject<Building>();
    b->SetBoundaries(Box(1, 3, 1, 4, 1, 3));
    b->SetNFloors(2);
    b->SetNRoomsX(2);
    b->SetNRoomsY(3);

    NodeContainer nodes;
    nodes.Create(24);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<PositionAllocator> positionAlloc = CreateObject<RandomRoomPositionAllocator>();
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);
    BuildingsHelper::Install(nodes);

    std::map<Room, uint32_t> roomCounter;

    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Ptr<MobilityModel> mm = (*it)->GetObject<MobilityModel>();
        NS_ASSERT_MSG(mm, "no mobility model aggregated to this node");
        Ptr<MobilityBuildingInfo> bmm = mm->GetObject<MobilityBuildingInfo>();
        NS_ASSERT_MSG(bmm,
                      "MobilityBuildingInfo has not been aggregated to this node mobility model");

        NS_TEST_ASSERT_MSG_EQ(bmm->IsIndoor(), true, "node should be indoor");
        Room r(bmm->GetRoomNumberX(), bmm->GetRoomNumberY(), bmm->GetFloorNumber());
        ++(roomCounter[r]);

        Vector p = mm->GetPosition();
        NS_TEST_ASSERT_MSG_GT(p.x, bmm->GetRoomNumberX(), "wrong x value");
        NS_TEST_ASSERT_MSG_LT(p.x, bmm->GetRoomNumberX() + 1, "wrong x value");
        NS_TEST_ASSERT_MSG_GT(p.y, bmm->GetRoomNumberY(), "wrong y value");
        NS_TEST_ASSERT_MSG_LT(p.y, bmm->GetRoomNumberY() + 1, "wrong y value");
        NS_TEST_ASSERT_MSG_GT(p.z, bmm->GetFloorNumber(), "wrong z value");
        NS_TEST_ASSERT_MSG_LT(p.z, bmm->GetFloorNumber() + 1, "wrong z value");
    }

    for (auto it = roomCounter.begin(); it != roomCounter.end(); ++it)
    {
        // random selection is done without replacement until the set of
        // eligible room is empty, at which point the set is filled
        // again. Hence with 24 nodes and 12 rooms we expect 2 nodes per room
        NS_TEST_ASSERT_MSG_EQ(it->second, 2, "expected 2 nodes per room");
    }

    NS_TEST_ASSERT_MSG_EQ(roomCounter.size(), 12, "expected 12 rooms allocated");

    Simulator::Destroy();
}

/**
 * @ingroup building-test
 *
 * SameRoomPositionAllocator test
 */
class SameRoomPositionAllocatorTestCase : public TestCase
{
  public:
    SameRoomPositionAllocatorTestCase();

  private:
    void DoRun() override;
};

SameRoomPositionAllocatorTestCase::SameRoomPositionAllocatorTestCase()
    : TestCase("SameRoom 48 nodes")
{
}

void
SameRoomPositionAllocatorTestCase::DoRun()
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("create building");
    Ptr<Building> b = CreateObject<Building>();
    b->SetBoundaries(Box(-10, -6, 20, 26, -1, 5));
    b->SetNFloors(2);
    b->SetNRoomsX(2);
    b->SetNRoomsY(3);

    NodeContainer nodes;
    nodes.Create(24);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<PositionAllocator> positionAlloc = CreateObject<RandomRoomPositionAllocator>();
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);
    BuildingsHelper::Install(nodes);

    NodeContainer copyNodes;
    copyNodes.Create(48);
    positionAlloc = CreateObject<SameRoomPositionAllocator>(nodes);
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(copyNodes);
    BuildingsHelper::Install(copyNodes);

    std::map<Room, uint32_t> roomCounter;

    for (auto it = copyNodes.Begin(); it != copyNodes.End(); ++it)
    {
        Ptr<MobilityModel> mm = (*it)->GetObject<MobilityModel>();
        NS_ASSERT_MSG(mm, "no mobility model aggregated to this node");
        Ptr<MobilityBuildingInfo> bmm = mm->GetObject<MobilityBuildingInfo>();
        NS_ASSERT_MSG(bmm,
                      "MobilityBuildingInfo has not been aggregated to this node mobility model");

        NS_TEST_ASSERT_MSG_EQ(bmm->IsIndoor(), true, "node should be indoor");
        Room r(bmm->GetRoomNumberX(), bmm->GetRoomNumberY(), bmm->GetFloorNumber());
        ++(roomCounter[r]);
    }

    for (auto it = roomCounter.begin(); it != roomCounter.end(); ++it)
    {
        NS_TEST_ASSERT_MSG_EQ(it->second, 4, "expected 4 nodes per room");
    }

    NS_TEST_ASSERT_MSG_EQ(roomCounter.size(), 12, "expected 12 rooms allocated");

    Simulator::Destroy();
}

/**
 * @ingroup building-test
 *
 * @brief RandomRoomPositionAllocator TestSuite
 */
class BuildingPositionAllocatorTestSuite : public TestSuite
{
  public:
    BuildingPositionAllocatorTestSuite();
};

BuildingPositionAllocatorTestSuite::BuildingPositionAllocatorTestSuite()
    : TestSuite("building-position-allocator", Type::UNIT)
{
    NS_LOG_FUNCTION(this);

    AddTestCase(new RandomRoomPositionAllocatorTestCase, TestCase::Duration::QUICK);
    AddTestCase(new SameRoomPositionAllocatorTestCase, TestCase::Duration::QUICK);
}

/// Static variable for test initialization
static BuildingPositionAllocatorTestSuite buildingsPositionAllocatorTestSuiteInstance;
