#include <iostream>
#include <unordered_map>

#include "gtest/gtest.h"

#include "ecs/EntityManager.hh"
#include "ecs/Entity.hh"

typedef struct Position
{
	Position() {}
	Position(int x, int y) : x(x), y(y) {}
	int x;
	int y;
} Position;

typedef struct Eater
{
    bool hungry;
    uint thingsEaten;
} Eater;

class ecs : public ::testing::Test
{
protected:
    sp::EntityManager em;
    sp::Entity e;

    virtual void SetUp()
    {
        e = em.NewEntity();
    }
};

TEST_F(ecs, CreateDestroyEntity)
{
    EXPECT_TRUE(em.Valid(e));
    em.Destroy(e);

    EXPECT_FALSE(em.Valid(e));
}

TEST_F(ecs, AddRemoveComponent)
{
	em.Assign<Position>(e);

	ASSERT_TRUE(em.Has<Position>(e));

	em.Remove<Position>(e);

	ASSERT_FALSE(em.Has<Position>(e));
    ASSERT_ANY_THROW(em.Get<Position>(e));
}

TEST_F(ecs, ConstructComponent)
{
    em.Assign<Position>(e, 1, 2);
    Position *pos = em.Get<Position>(e);

    ASSERT_NE(pos, nullptr);
    ASSERT_EQ(pos->x, 1);
    ASSERT_EQ(pos->y, 2);
}

TEST_F(ecs, RemoveAllComponents)
{
    em.Assign<Position>(e);
    em.Assign<Eater>(e);

    ASSERT_TRUE(em.Has<Position>(e));
    ASSERT_TRUE(em.Has<Eater>(e));

    em.RemoveAllComponents(e);

    ASSERT_FALSE(em.Has<Position>(e));
    ASSERT_FALSE(em.Has<Eater>(e));
}

TEST_F(ecs, IterateOverEntitiesWithComponents)
{
	sp::Entity ePos1 = em.NewEntity();
	sp::Entity ePos2 = em.NewEntity();
	sp::Entity ePosEat = em.NewEntity();
	sp::Entity eEat = em.NewEntity();
	sp::Entity eNoComps = em.NewEntity();

	em.Assign<Position>(ePos1);
	em.Assign<Position>(ePos2);
	em.Assign<Position>(ePosEat);

	em.Assign<Eater>(ePosEat);
	em.Assign<Eater>(eEat);

	std::unordered_map<sp::Entity, bool> entsFound;

	em.EachWith<Position>([&entsFound](sp::Entity _e, Position *pos)
	{
		entsFound[_e] = true;
	});

	// found entities with the component
	EXPECT_TRUE(entsFound.count(ePos1) == 1 && entsFound[ePos1] == true);
	EXPECT_TRUE(entsFound.count(ePos2) == 1 && entsFound[ePos2] == true);
	EXPECT_TRUE(entsFound.count(ePosEat) == 1 && entsFound[ePosEat] == true);

	// did not find entities without the component
	EXPECT_FALSE(entsFound.count(eEat) == 1 && entsFound[eEat] == true);
	EXPECT_FALSE(entsFound.count(eNoComps) == 1 && entsFound[eNoComps] == true);
}