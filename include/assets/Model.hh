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

		struct Attribute
		{
			size_t byteOffset, byteStride;
			int componentType, componentCount;
			string bufferName;
		};

		struct Primitive
		{
			Node node;
			size_t elementCount;
			string indexBuffer;
			size_t indexOffset;
			Attribute attributes[3];
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

		class primitive_iterator
		{
		private:
			primitive_iterator(primitive_iterator *copy, vector<Primitive>::iterator pos) : primitives(copy->primitives), p_iter(pos), scene(copy->scene), node(copy->node) {}

			void increment();

			vector<Primitive> primitives;
			vector<Primitive>::iterator p_iter;
			tinygltf::Scene *scene;
			Node *node;

		public:
			primitive_iterator(tinygltf::Scene *scene, Node *node);

			primitive_iterator begin();
			primitive_iterator end();

			primitive_iterator operator++()
			{
				primitive_iterator i = *this;
				p_iter++;
				return i;
			}
			primitive_iterator operator++(int i)
			{
				p_iter++;
				return *this;
			}

			bool operator==(primitive_iterator const &other) const
			{
				return this->p_iter == other.p_iter;
			}
			bool operator!=(primitive_iterator const &other) const
			{
				return this->p_iter != other.p_iter;
			}

			Primitive const &operator*() const
			{
				return *p_iter;
			}
			Primitive const *operator->() const
			{
				return &(*p_iter);
			}
		};

		node_iterator ListNodes();
		primitive_iterator ListPrimitives(Node *node);
	private:
		shared_ptr<Asset> asset;
	};
}

#endif
