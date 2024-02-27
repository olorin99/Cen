#include "BloomPass.h"
#include <Ende/util/colour.h>

auto cen::passes::bloom(canta::RenderGraph &graph, cen::passes::BloomParams params) -> canta::ImageIndex {
    auto bloomGroup = graph.getGroup("bloom", ende::util::rgb(196, 187, 11));

    auto bloomDownsampleIndex = graph.addImage({
        .matchesBackbuffer = false,
        .width = params.width / 2,
        .height = params.height / 2,
        .mipLevels = static_cast<u32>(params.mips),
        .format = canta::Format::RGBA32_SFLOAT,
        .name = "bloom_downsample"
    });
    auto bloomUpsampleIndex = graph.addImage({
        .matchesBackbuffer = false,
        .width = params.width,
        .height = params.height,
        .mipLevels = static_cast<u32>(params.mips),
        .format = canta::Format::RGBA32_SFLOAT,
        .name = "bloom_upsample"
    });
    auto bloomOutput = graph.addImage({
        .matchesBackbuffer = true,
        .format = canta::Format::RGBA32_SFLOAT,
        .name = "bloom_final"
    });

    auto downsampleInput = graph.addAlias(bloomDownsampleIndex);
    for (u32 i = 0; i < params.mips; i++) {
        auto downsampleOutput = graph.addAlias(bloomDownsampleIndex);
        graph.addPass(std::format("bloom_downsample_{}", i), canta::PassType::COMPUTE, bloomGroup)
            .addSampledRead(i == 0 ? params.hdrBackbuffer : downsampleInput, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageImageWrite(downsampleOutput, canta::PipelineStage::COMPUTE_SHADER)
            .setExecuteFunction([params, i, downsampleInput, downsampleOutput] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto inputImage = graph.getImage(i == 0 ? params.hdrBackbuffer : downsampleInput);
                auto outputImage = graph.getImage(downsampleOutput);

                cmd.bindPipeline(params.downsamplePipeline);
                struct Push {
                    i32 input;
                    i32 output;
                    i32 bilinearSampler;
                    i32 mipLevel;
                };
                cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .input = inputImage->mipView(i == 0 ? 0 : i - 1).index(),
                    .output = outputImage->mipView(i).index(),
                    .bilinearSampler = params.bilinearSampler.index(),
                    .mipLevel = static_cast<i32>(i)
                });

                cmd.dispatchThreads(inputImage->width() >> i, inputImage->height() >> i);
            });
        downsampleInput = downsampleOutput;
    }

    auto upsampleInput = graph.addAlias(bloomUpsampleIndex);
    for (i32 i = params.mips - 1; i > -1; i--) {
        auto upsampleOutput = graph.addAlias(bloomUpsampleIndex);
        graph.addPass(std::format("bloom_upsample_{}", i), canta::PassType::COMPUTE, bloomGroup)
            .addSampledRead(i == (params.mips - 1) ? downsampleInput : upsampleInput, canta::PipelineStage::COMPUTE_SHADER)
            .addSampledRead(downsampleInput, canta::PipelineStage::COMPUTE_SHADER)
            .addStorageImageWrite(upsampleOutput, canta::PipelineStage::COMPUTE_SHADER)
            .setExecuteFunction([params, i, upsampleInput, upsampleOutput, downsampleInput] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
                auto downsampleImage = graph.getImage(downsampleInput);
                auto inputImage = graph.getImage(i == (params.mips - 1) ? downsampleInput : upsampleInput);
                auto outputImage = graph.getImage(upsampleOutput);

                cmd.bindPipeline(params.upsamplePipeline);
                struct Push {
                    i32 input;
                    i32 sum;
                    i32 output;
                    i32 bilinearSampler;
                };
                cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                    .input = inputImage->mipView(i == (params.mips - 1) ? i  : i + 1).index(),
                    .sum = i == (params.mips - 1) ? -1 : downsampleImage->mipView(i).index(),
                    .output = outputImage->mipView(i).index(),
                    .bilinearSampler = params.bilinearSampler.index(),
                });
                cmd.dispatchThreads(outputImage->width() >> i, outputImage->height() >> i);
            });
        upsampleInput = upsampleOutput;
    }

    graph.addPass("bloom_composite", canta::PassType::COMPUTE, bloomGroup)
        .addStorageImageRead(upsampleInput, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageImageRead(params.hdrBackbuffer, canta::PipelineStage::COMPUTE_SHADER)
        .addStorageImageWrite(bloomOutput, canta::PipelineStage::COMPUTE_SHADER)
        .setExecuteFunction([params, upsampleInput, bloomOutput] (canta::CommandBuffer& cmd, canta::RenderGraph& graph) {
            auto globalBuffer = graph.getBuffer(params.globalBuffer);
            auto upsampleImage = graph.getImage(upsampleInput);
            auto hdrImage = graph.getImage(params.hdrBackbuffer);
            auto outputImage = graph.getImage(bloomOutput);

            cmd.bindPipeline(params.compositePipeline);
            struct Push {
                u64 globalBuffer;
                i32 upsample;
                i32 hdr;
                i32 output;
                i32 mipCount;
            };
            cmd.pushConstants(canta::ShaderStage::COMPUTE, Push {
                .globalBuffer = globalBuffer->address(),
                .upsample = upsampleImage->defaultView().index(),
                .hdr = hdrImage->defaultView().index(),
                .output = outputImage->defaultView().index(),
                .mipCount = params.mips
            });
            cmd.dispatchThreads(outputImage->width(), outputImage->height());
        });
    return bloomOutput;
}