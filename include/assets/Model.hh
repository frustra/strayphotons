#ifndef SP_MODEL_H
#define SP_MODEL_H

#include "Common.hh"
#include <tiny_gltf_loader.h>
#include <glm/glm.hpp>

namespace sp
{
	class Asset;

	class Model : public NonCopyable
	{
	public:
		Model(const string &name, shared_ptr<Asset> asset);
		~Model();

		const string name;
		tinygltf::Scene scene;

		struct Node
		{
			glm::mat4 matrix;
			tinygltf::Node *node;
		};

		class node_iterator
		{
		private:
			node_iterator(node_iterator *copy, vector<Node>::iterator pos) : nodes(copy->nodes), n_iter(pos), scene(copy->scene) {}

			void increment();

			vector<Node> nodes;
			vector<Node>::iterator n_iter;
			tinygltf::Scene *scene;

		public:
			node_iterator(tinygltf::Scene *s);

			node_iterator begin();
			node_iterator end();

			node_iterator operator++()
			{
				node_iterator i = *this;
				increment();
				return i;
			}
			node_iterator operator++(int i)
			{
				increment();
				return *this;
			}

			bool operator==(node_iterator const &other) const
			{
				return this->n_iter == other.n_iter;
			}
			bool operator!=(node_iterator const &other) const
			{
				return this->n_iter != other.n_iter;
			}

			Node const &operator*() const
			{
				return *n_iter;
			}
			Node const *operator->() const
			{
				return &(*n_iter);
			}
		};

		node_iterator list_nodes();
	private:
		shared_ptr<Asset> asset;
	};
}

#endif
