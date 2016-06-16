#include <iostream>
#include <unordered_map>
#include <stdexcept>

#include "gtest/gtest.h"

#include "ecs/Ecs.hh"

namespace test
{

	typedef struct Position
	{
		Position() {}
		Position(int x, int y) : x(x), y(y) {}
		bool operator==(const Position & other) const { return x == other.x && y == other.y; }
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
		std::unordered_map<ecs::Entity, bool> entsFound;

		ecs::EntityManager em;
		ecs::Entity ePos1;
		ecs::Entity ePos2;
		ecs::Entity ePosEat;
		ecs::Entity eEat;
		ecs::Entity eNoComps;

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

		void ExpectPositionEntitiesFound()
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

	TEST(EcsBasic, CreateDestroyEntity)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		EXPECT_TRUE(e.Valid());
		e.Destroy();

		EXPECT_FALSE(e.Valid());
	}

	TEST(EcsBasic, AddRemoveComponent)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.Assign<Position>();

		ASSERT_TRUE(e.Has<Position>());

		e.Remove<Position>();

		ASSERT_FALSE(e.Has<Position>());
		ASSERT_ANY_THROW(e.Get<Position>());
	}

	TEST(EcsBasic, ConstructComponent)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.Assign<Position>(1, 2);
		ecs::Handle<Position> pos = e.Get<Position>();

		ASSERT_EQ(pos->x, 1);
		ASSERT_EQ(pos->y, 2);
	}

	TEST(EcsBasic, RemoveAllComponents)
	{
		ecs::EntityManager em;
		ecs::Entity e = em.NewEntity();

		e.Assign<Position>();
		e.Assign<Eater>();

		ASSERT_TRUE(e.Has<Position>());
		ASSERT_TRUE(e.Has<Eater>());

		e.RemoveAllComponents();

		ASSERT_FALSE(e.Has<Position>());
		ASSERT_FALSE(e.Has<Eater>());
	}

	TEST_F(EcsBasicIterateWithComponents, MultiComponentTemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith<Eater, Position>())
		{
			entsFound[ent] = true;
		}

		EXPECT_TRUE(entsFound.count(ePosEat) == 1 && entsFound[ePosEat] == true);
		EXPECT_EQ(1, entsFound.size()) << "should have only found one entity";
	}

	TEST_F(EcsBasicIterateWithComponents, TemplateIteration)
	{
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			entsFound[ent] = true;
		}

		ExpectPositionEntitiesFound();
	}

	TEST_F(EcsBasicIterateWithComponents, MaskIteration)
	{
		auto compMask = em.CreateComponentMask<Position>();
		for (ecs::Entity ent : em.EntitiesWith(compMask))
		{
			entsFound[ent] = true;
		}

		ExpectPositionEntitiesFound();
	}

	TEST(EcsBasic, AddEntitiesWhileIterating)
	{
		ecs::EntityManager em;
		ecs::Entity e1 = em.NewEntity();
		e1.Assign<Position>();

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith<Position>())
		{
			ent.Valid(); // prevent -Wunused-but-set-variable
			entitiesFound++;
			if (entitiesFound == 1)
			{
				ecs::Entity e2 = em.NewEntity();
				e2.Assign<Position>();
			}
		}

		ASSERT_EQ(1, entitiesFound) << "Should have only found the entity created before started iterating";
	}

	// test to verify that an entity is not iterated over if it is destroyed
	// before we get to it during iteration
	TEST(EcsBasic, RemoveEntityWhileIterating)
	{
		ecs::EntityManager em;

		ecs::Entity e1 = em.NewEntity();
		e1.Assign<Position>();

		ecs::Entity e2 = em.NewEntity();
		e2.Assign<Position>();

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith<Position>())
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
		ecs::EntityManager em;

		ecs::Entity e1 = em.NewEntity();
		e1.Assign<Position>();

		ecs::Entity e2 = em.NewEntity();
		e2.Assign<Position>();

		int entitiesFound = 0;
		for (ecs::Entity ent : em.EntitiesWith<Position>())
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

	TEST(EcsBasic, RegisterComponentPreventsExceptions)
	{
		ecs::EntityManager em;

		ecs::Entity e = em.NewEntity();

		// assert that exceptions are raised before registering
		ASSERT_THROW(e.Has<Position>(), std::invalid_argument);
		ASSERT_THROW(
			for (auto e : em.EntitiesWith<Position>()) {
				e.Valid();
			},
			std::invalid_argument
		);

		em.RegisterComponentType<Position>();

		// assert that exceptions no longer occur
		ASSERT_NO_THROW(e.Has<Position>());

		ASSERT_NO_THROW(
			for (auto e : em.EntitiesWith<Position>()) {
				e.Valid();
			}
		);
	}

	TEST(EcsBasic, DeleteComponentDoesNotInvalidateOtherComponents)
	{
		ecs::EntityManager em;
		auto e1 = em.NewEntity();
		auto e2 = em.NewEntity();

		e1.Assign<Position>(1, 1);
		e2.Assign<Position>(2, 2);

		ecs::Handle<Position> p2Handle = e2.Get<Position>();

		Position p2before = *p2Handle;
		e1.Remove<Position>();
		Position p2now = *p2Handle;

		ASSERT_EQ(p2before, p2now);
	}
}
