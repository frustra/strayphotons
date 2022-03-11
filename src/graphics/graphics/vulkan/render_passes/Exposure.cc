#include "Exposure.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    // clang-format off
    static CVar<float> CVarExposure("r.Exposure", 0.0, "Fixed exposure value in linear units (0: auto)");
    static CVar<float> CVarExposureMin("r.ExposureMin", 0.01, "Minimum linear exposure value (controls max brightness)");
    static CVar<float> CVarExposureMax("r.ExposureMax", 10, "Maximum linear exposure value (controls min brightness)");
    static CVar<float> CVarExposureComp("r.ExposureComp", 3, "Exposure bias in EV units (logarithmic) for eye adaptation");
    static CVar<float> CVarEyeAdaptationLow("r.EyeAdaptationLow", 65, "Percent of darkest pixels to ignore in eye adaptation");
    static CVar<float> CVarEyeAdaptationHigh("r.EyeAdaptationHigh", 92, "Percent of brightest pixels to ignore in eye adaptation");
    static CVar<float> CVarEyeAdaptationMinLuminance("r.EyeAdaptationMinLuminance", 0.01, "Minimum target luminance for eye adaptation");
    static CVar<float> CVarEyeAdaptationMaxLuminance("r.EyeAdaptationMaxLuminance", 10000, "Maximum target luminance for eye adaptation");
    static CVar<float> CVarEyeAdaptationUpRate("r.EyeAdaptationUpRate", 0.1, "Rate at which eye adapts to brighter scenes");
    static CVar<float> CVarEyeAdaptationDownRate("r.EyeAdaptationDownRate", 0.04, "Rate at which eye adapts to darker scenes");
    static CVar<float> CVarEyeAdaptationKeyComp("r.EyeAdaptationKeyComp", 1.0, "Amount of key compensation for eye adaptation (0-1)");
    static CVar<bool> CVarHistogram("r.Histogram", false, "Overlay luminance histogram in view");
    // clang-format on

    struct ExposureUpdateParams {
        float exposureMin;
        float exposureMax;
        float exposureComp;
        float eyeAdaptationLow;
        float eyeAdaptationHigh;
        float eyeAdaptationMinLuminance;
        float eyeAdaptationMaxLuminance;
        float eyeAdaptationUpRate;
        float eyeAdaptationDownRate;
        float eyeAdaptationKeyComp;
    };

    struct ExposureState {
        float exposure = 1.0f;
    };

    void AddExposureState(RenderGraph &graph) {
        graph.AddPass("ExposureState")
            .Build([&](rg::PassBuilder &builder) {
                builder.BufferCreate("ExposureState",
                    sizeof(ExposureState),
                    BufferUsage::eTransferDst,
                    Residency::CPU_TO_GPU);

                auto lastStateID = builder.PreviousFrame().GetID("NextExposureState");
                if (lastStateID != InvalidResource) builder.TransferRead(lastStateID);
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                auto stateBuffer = resources.GetBuffer("ExposureState");

                ExposureState state;
                auto manualExposure = CVarExposure.Get();
                if (manualExposure > 0) state.exposure = manualExposure;

                auto lastStateID = resources.GetID("NextExposureState", false, 1);
                if (lastStateID == InvalidResource || manualExposure > 0) {
                    stateBuffer->CopyFrom(&state);
                } else {
                    auto lastStateBuffer = resources.GetBuffer(lastStateID);
                    vk::BufferCopy region(0, 0, lastStateBuffer->Size());
                    cmd.Raw().copyBuffer(*lastStateBuffer, *stateBuffer, {region});
                }
            });
    }

    void AddExposureUpdate(RenderGraph &graph) {
        const int bins = 64;
        const int wgsize = 16;
        const int downsample = 2; // read every N pixels

        auto source = graph.LastOutputID();

        graph.AddPass("LuminanceHistogram")
            .Build([&](rg::PassBuilder &builder) {
                builder.TextureRead(source);

                RenderTargetDesc histogramDesc;
                histogramDesc.extent = vk::Extent3D(bins, 1, 1);
                histogramDesc.format = vk::Format::eR32Uint;
                histogramDesc.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
                builder.RenderTargetCreate("LuminanceHistogram", histogramDesc);
            })
            .Execute([source](rg::Resources &resources, CommandContext &cmd) {
                auto luminance = resources.GetRenderTarget(source)->LayerImageView(0);
                auto histogram = resources.GetRenderTarget("LuminanceHistogram");

                vk::ClearColorValue clear;
                vk::ImageSubresourceRange range;
                range.layerCount = 1;
                range.levelCount = 1;
                range.aspectMask = vk::ImageAspectFlagBits::eColor;
                cmd.ImageBarrier(histogram->ImageView()->Image(),
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eGeneral,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    {},
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite);
                cmd.Raw().clearColorImage(*histogram->ImageView()->Image(), vk::ImageLayout::eGeneral, clear, {range});

                cmd.SetComputeShader("lumi_histogram.comp");
                cmd.SetImageView(0, 0, luminance);
                cmd.SetImageView(0, 1, histogram->ImageView());

                auto width = luminance->Extent().width / downsample;
                auto height = luminance->Extent().height / downsample;
                cmd.Dispatch((width + wgsize - 1) / wgsize, (height + wgsize - 1) / wgsize, 1);
            });

        graph.AddPass("ExposureUpdate")
            .Build([&](rg::PassBuilder &builder) {
                builder.StorageRead("LuminanceHistogram");
                builder.StorageRead("ExposureState");

                builder.BufferCreate("NextExposureState",
                    sizeof(ExposureState),
                    BufferUsage::eStorageBuffer | BufferUsage::eTransferSrc,
                    Residency::CPU_TO_GPU);
                builder.ExportToNextFrame("NextExposureState");
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                ExposureUpdateParams constants;
                constants.exposureMin = CVarExposureMin.Get();
                constants.exposureMax = CVarExposureMax.Get();
                constants.exposureComp = CVarExposureComp.Get();
                constants.eyeAdaptationLow = CVarEyeAdaptationLow.Get();
                constants.eyeAdaptationHigh = CVarEyeAdaptationHigh.Get();
                constants.eyeAdaptationMinLuminance = CVarEyeAdaptationMinLuminance.Get();
                constants.eyeAdaptationMaxLuminance = CVarEyeAdaptationMaxLuminance.Get();
                constants.eyeAdaptationUpRate = CVarEyeAdaptationUpRate.Get();
                constants.eyeAdaptationDownRate = CVarEyeAdaptationDownRate.Get();
                constants.eyeAdaptationKeyComp = CVarEyeAdaptationKeyComp.Get();

                cmd.SetComputeShader("exposure_update.comp");
                cmd.SetImageView(0, 0, resources.GetRenderTarget("LuminanceHistogram")->ImageView());
                cmd.SetStorageBuffer(0, 1, resources.GetBuffer("ExposureState"));
                cmd.SetStorageBuffer(0, 2, resources.GetBuffer("NextExposureState"));
                cmd.PushConstants(constants);
                cmd.Dispatch(1, 1, 1);
            });

        if (CVarHistogram.Get())
            graph.AddPass("ViewHistogram")
                .Build([&](rg::PassBuilder &builder) {
                    builder.StorageRead("LuminanceHistogram");
                    auto lumi = builder.TextureRead(source);

                    auto desc = lumi.DeriveRenderTarget();
                    builder.OutputColorAttachment(0, "ViewH", desc, {LoadOp::DontCare, StoreOp::Store});
                })
                .Execute([source](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetShaders("screen_cover.vert", "render_histogram.frag");
                    cmd.SetImageView(0, 0, resources.GetRenderTarget(source)->ImageView());
                    cmd.SetImageView(0, 1, resources.GetRenderTarget("LuminanceHistogram")->ImageView());
                    cmd.Draw(3);
                });
    }
} // namespace sp::vulkan::renderer
