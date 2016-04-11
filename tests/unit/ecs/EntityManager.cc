#include "gtest/gtest.h"

#include "ecs/EntityManager.hh"
#include "ecs/Entity.hh"

typedef struct {
	int x, y;
} Position;

TEST(EntityManager, CreateDestroyEntity)
{
	auto em = sp::EntityManager();
	sp::Entity e = em.NewEntity();

	EXPECT_TRUE(em.Valid(e));
	em.Destroy(e);

	EXPECT_FALSE(em.Valid(e));
}

TEST(EntityManager, AddRemoveComponent)
{
	auto em = sp::EntityManager();
	sp::Entity e = em.NewEntity();

	em.Assign<Position>(e);

	bool hasComp = em.Has<Position>(e);
	EXPECT_TRUE(hasComp);

	em.Remove<Position>(e);

	hasComp = em.Has<Position>(e);
	EXPECT_FALSE(hasComp);
}