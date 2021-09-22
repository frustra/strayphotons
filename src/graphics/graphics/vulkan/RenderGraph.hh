#include "Common.hh"
#include "RenderTarget.hh"
#include "core/StackVector.hh"

namespace sp::vulkan {
    struct RenderGraphResource {
        enum class Type {
            Undefined,
            RenderTarget,
        };

        RenderGraphResource() {}
        RenderGraphResource(RenderTargetDesc desc) : type(Type::RenderTarget), renderTargetDesc(desc) {}

        uint32 id = ~0u;
        Type type = Type::Undefined;
        union {
            RenderTargetDesc renderTargetDesc;
        };
    };

    class RenderGraphResources {
    public:
        RenderGraphResources(DeviceContext &device) : device(device) {}

        RenderTargetPtr GetRenderTarget(const RenderGraphResource &res);

        const RenderGraphResource &GetResourceByName(string_view name) const;

    private:
        friend class RenderGraph;
        friend class RenderGraphPassBuilder;

        void ResizeBeforeExecute();
        void IncrementRef(uint32 id);
        void DecrementRef(uint32 id);

        void Register(string_view name, RenderGraphResource &resource) {
            // Returns ID used to populate a RenderGraphResource
            resource.id = (uint32)resources.size();
            resources.push_back(resource);
            names.push_back(name);
        }

        DeviceContext &device;
        vector<string_view> names;
        vector<RenderGraphResource> resources;

        // Built during execution
        vector<uint32> refCounts;
        vector<RenderTargetPtr> renderTargets;
    };

    class RenderGraphPassBase {
    public:
        RenderGraphPassBase(string_view name) : name(name) {}
        virtual ~RenderGraphPassBase() {}
        virtual void Execute(RenderGraphResources &resources, CommandContext &cmd) = 0;

        void AddDependency(const RenderGraphResource &res) {
            dependencies.push(res.id);
        }

        void AddOutput(const RenderGraphResource &res) {
            outputs.push(res.id);
        }

    private:
        friend class RenderGraph;
        string_view name;
        StackVector<uint32, 32> dependencies;
        StackVector<uint32, 16> outputs;
    };

    template<typename DataType, typename ExecuteFunc>
    class RenderGraphPass : public RenderGraphPassBase {
    public:
        RenderGraphPass(string_view name, ExecuteFunc &executeFunc)
            : RenderGraphPassBase(name), executeFunc(executeFunc) {}

        DataType data;

        void Execute(RenderGraphResources &resources, CommandContext &cmd) override {
            executeFunc(resources, cmd, data);
        }

    private:
        ExecuteFunc executeFunc;
    };

    class RenderGraphPassBuilder {
    public:
        RenderGraphPassBuilder(RenderGraphResources &resources, RenderGraphPassBase &pass)
            : resources(resources), pass(pass) {}

        RenderGraphResource Read(const RenderGraphResource &input) {
            pass.AddDependency(input);
            return input;
        }

        RenderGraphResource GetResourceByName(string_view name) {
            return resources.GetResourceByName(name);
        }

        RenderGraphResource OutputRenderTarget(string_view name, const RenderTargetDesc &desc) {
            auto resource = RenderGraphResource(desc);
            resources.Register(name, resource);
            pass.AddOutput(resource);
            return resource;
        }

    private:
        RenderGraphResources &resources;
        RenderGraphPassBase &pass;
    };

    class RenderGraph {
    public:
        RenderGraph(DeviceContext &device) : device(device), resources(device) {}

        template<typename DataType, typename SetupFunc, typename ExecuteFunc>
        DataType &AddPass(string_view name, SetupFunc setupFunc, ExecuteFunc executeFunc) {
            static_assert(sizeof(ExecuteFunc) < 1024, "execute callback must capture less than 1kB");

            auto pass = make_shared<RenderGraphPass<DataType, ExecuteFunc>>(name, executeFunc);
            RenderGraphPassBuilder builder(resources, *pass);
            setupFunc(builder, pass->data);
            passes.push_back(pass);
            return pass->data;
        }

        void Execute();

    private:
        DeviceContext &device;
        RenderGraphResources resources;
        vector<shared_ptr<RenderGraphPassBase>> passes;
    };
} // namespace sp::vulkan
