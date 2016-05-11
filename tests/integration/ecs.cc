#include <iostream>
#include <unordered_map>

#include "gtest/gtest.h"

#include "ecs/Ecs.hh"

namespace test
{

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

	class EcsBasicIterateWithComponents : public ::testing::Test
	{
	protected:
		std::unordered_map<sp::Entity, bool> entsFound;

	    sp::EntityManager em;
		sp::Entity ePos1;
		sp::Entity ePos2;
		sp::Entity ePosEat;
		sp::Entity eEat;
		sp::Entity eNoComps;

	    virtual void SetUp()
	    {
			ePos1 = em.NewEntity();
			ePos2 = em.NewEntity();
			ePosEat = em.NewEntity();
			eEat = em.NewEntity();
			eNoComps = em.NewEntity();

			ePos1.Assign<Position>();
			ePos2.Assign<Position>();
			ePosEat.Assign<Position>();

			ePosEat.Assign<Eater>();
			eEat.Assign<Eater>();
	    }

		void ExpectEntitiesFound()
		{
			// found entities with the component
			EXPECT_TRUE(entsFound.count(ePos1) == 1 && entsFound[ePos1] == true);
			EXPECT_TRUE(entsFound.count(ePos2) == 1 && entsFound[ePos2] == true);
			EXPECT_TRUE(entsFound.count(ePosEat) == 1 && entsFound[ePosEat] == true);

			// did not find entities without the component
			EXPECT_FALSE(entsFound.count(eEat) == 1 && entsFound[eEat] == true);
			EXPECT_FALSE(entsFound.count(eNoComps) == 1 && entsFound[eNoComps] == true);
		}
	};

	// class EcsBasic : public ::testing::Test
	// {
	// protected:
	//     sp::EntityManager em;
	//     sp::Entity e;
	//
	//     virtual void SetUp()
	//     {
	//         e = em.NewEntity();
	//     }
	// };


	TEST(EcsBasic, CreateDestroyEntity)
	{
		sp::EntityManager em;
	    sp::Entity e = em.NewEntity();

	    EXPECT_TRUE(e.Valid());
	    e.Destroy();

	    EXPECT_FALSE(e.Valid());
	}

	TEST(EcsBasic, AddRemoveComponent)
	{
		sp::EntityManager em;
	    sp::Entity e = em.NewEntity();

		e.Assign<Position>();

		ASSERT_TRUE(e.Has<Position>());

		e.Remove<Position>();

		ASSERT_FALSE(e.Has<Position>());
	    ASSERT_ANY_THROW(e.Get<Position>());
	}

	TEST(EcsBasic, ConstructComponent)
	{
		sp::EntityManager em;
	    sp::Entity e = em.NewEntity();

	    e.Assign<Position>(1, 2);
	    Position *pos = e.Get<Position>();

	    ASSERT_NE(pos, nullptr);
	    ASSERT_EQ(pos->x, 1);
	    ASSERT_EQ(pos->y, 2);
	}

	TEST(EcsBasic, RemoveAllComponents)
	{
		sp::EntityManager em;
	    sp::Entity e = em.NewEntity();

	    e.Assign<Position>();
	    e.Assign<Eater>();

	    ASSERT_TRUE(e.Has<Position>());
	    ASSERT_TRUE(e.Has<Eater>());

	    e.RemoveAllComponents();

	    ASSERT_FALSE(e.Has<Position>());
	    ASSERT_FALSE(e.Has<Eater>());
	}

	TEST_F(EcsBasicIterateWithComponents, TemplateIteration)
	{
		for (sp::Entity ent : em.EntitiesWith<Position>())
		{
			entsFound[ent] = true;
		}

		ExpectEntitiesFound();
	}

	TEST_F(EcsBasicIterateWithComponents, MaskIteration)
	{
		auto compMask = em.CreateComponentMask<Position>();
		for (sp::Entity ent : em.EntitiesWith(compMask))
		{
			entsFound[ent] = true;
		}

		ExpectEntitiesFound();
	}

	TEST(EcsBasic, AddEntitiesWhileIterating)
	{
		sp::EntityManager em;
		sp::Entity e1 = em.NewEntity();
		e1.Assign<Position>();

		int entitiesFound = 0;
		for (sp::Entity ent : em.EntitiesWith<Position>())
		{
			entitiesFound++;
			if (entitiesFound == 1)
			{
				sp::Entity e2 = em.NewEntity();
				e2.Assign<Position>();
			}
		}

		ASSERT_EQ(1, entitiesFound) << "Should have only found the entity created before started iterating";
	}

	// test to verify that an entity is not iterated over if it is destroyed
	// before we get to it during iteration
	TEST(EcsBasic, RemoveEntityWhileIterating)
	{
		sp::EntityManager em;

		sp::Entity e1 = em.NewEntity();
		e1.Assign<Position>();

		sp::Entity e2 = em.NewEntity();
		e2.Assign<Position>();

		int entitiesFound = 0;
		for (sp::Entity ent : em.EntitiesWith<Position>())
		{
			entitiesFound++;
			if (ent == e1)
			{
				e2.Destroy();
			}
			else
			{
				e1.Destroy();
			}
		}

		ASSERT_EQ(1, entitiesFound) <<
			"Should have only found one entity because the other was destroyed";
	}

	// test to verify that an entity is not iterated over if it's component we are interested in
	// is removed before we get to it during iteration
	TEST(EcsBasic, RemoveComponentWhileIterating)
	{
		sp::EntityManager em;

		sp::Entity e1 = em.NewEntity();
		e1.Assign<Position>();

		sp::Entity e2 = em.NewEntity();
		e2.Assign<Position>();

		int entitiesFound = 0;
		for (sp::Entity ent : em.EntitiesWith<Position>())
		{
			entitiesFound++;
			if (ent == e1)
			{
				e2.Remove<Position>();
			}
			else
			{
				e1.Remove<Position>();
			}
		}

		ASSERT_EQ(1, entitiesFound) <<
			"Should have only found one entity because the other's component was removed before"
			" we got to it during iteration";
	}
}
