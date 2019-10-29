#include "RaytracedRenderer.hh"
#include "GPUTypes.hh"
#include "SceneContext.hh"
#include "graphics/Renderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "graphics/GPUTimer.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "core/Game.hh"
#include "core/CVar.hh"

#include <thread>
#include <cmath>
#include <algorithm>

namespace sp
{
	namespace raytrace
	{
		static CVar<float> CVarAperture("r.RayTrace.Aperture", 0.0f, "Raytracing camera aperture");
		static CVar<float> CVarFocalDist("r.RayTrace.FocalDist", 4.0f, "Raytracing camera focal distance");
		static CVar<int> CVarInvocationSize("r.RayTrace.InvocationSize", 128, "Raytracing shader invocation size");
		static CVar<bool> CVarDynamicView("r.RayTrace.Dynamic", true, "Dynamically update view while raytracing");
		static CVar<float> CVarExposure("r.RayTrace.Exposure", 0.0f, "Raytracing exposure lock");
		static CVar<bool> CVarBloom("r.RayTrace.Bloom", true, "Raytracing bloom enabled");
		static CVar<float> CVarBloomThreshold("r.RayTrace.BloomThreshold", 0.8f, "Raytracing bloom threshold");
		static CVar<bool> CVarTonemap("r.RayTrace.Tonemap", true, "Raytracing tone mapping");
		static CVar<bool> CVarPause("r.RayTrace.Pause", false, "Stop updating the result");

		class PathTraceSceneCS : public Shader
		{
			SHADER_TYPE(PathTraceSceneCS)

			PathTraceSceneCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
			{
				Bind(tanHalfFoV, "tanHalfFoV");
				Bind(eyePos, "eyePos");
				Bind(eyeRot, "eyeRot");
				Bind(seed, "seed");
				Bind(focalDist, "focalDist");
				Bind(aperture, "aperture");
				Bind(invocationOffset, "invocationOffset");

				BindBuffer(materialData, 0);
				BindBuffer(sceneData, 2);
				BindBuffer(vertexData, 0, GL_SHADER_STORAGE_BUFFER);
				BindBuffer(faceData, 1, GL_SHADER_STORAGE_BUFFER);
				BindBuffer(bvhData, 2, GL_SHADER_STORAGE_BUFFER);
			}

			void UpdateInvocation(glm::uvec2 offset)
			{
				Set(invocationOffset, offset);
			}

			void UpdateParameters(float fov, glm::vec3 eye, glm::mat3 rotMat)
			{
				Set(tanHalfFoV, (float)tan(fov / 2));
				Set(eyePos, eye);
				Set(eyeRot, rotMat);
				//Set(seed, (float)((double) rand() * 1000.0 / (double) RAND_MAX));
				Set(seed, prevSeed += 0.2f);
				Set(focalDist, CVarFocalDist.Get());
				Set(aperture, CVarAperture.Get());
			}

			void UpdateSceneData(GPUSceneContext &ctx)
			{
				BufferData(materialData, sizeof(GLMaterialDataBuffer), &ctx.matData);
				BufferData(sceneData, sizeof(GLSceneDataBuffer), &ctx.sceneData);
				BufferData(vertexData, sizeof(GLVertex) * ctx.vtxData.size(), ctx.vtxData.data());
				BufferData(faceData, sizeof(uint32) * ctx.faceData.size(), ctx.faceData.data());
				BufferData(bvhData, sizeof(GLBVHNode) * ctx.bvhData.size(), ctx.bvhData.data());
			}

		private:
			Uniform tanHalfFoV, seed, invocationOffset;
			Uniform eyePos, eyeRot, focalDist, aperture;

			UniformBuffer materialData, lightData, sceneData;
			StorageBuffer vertexData, faceData, bvhData;

			float prevSeed = 0.1f;
		};

		IMPLEMENT_SHADER_TYPE(PathTraceSceneCS, "raytrace/path_trace_scene.glsl", Compute);

		class RayTraceBloomThresholdFS : public Shader
		{
			SHADER_TYPE(RayTraceBloomThresholdFS)

			RayTraceBloomThresholdFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
			{
				Bind(threshold, "threshold");
			}

			void UpdateThreshold(float newThreshold)
			{
				Set(threshold, newThreshold);
			}

		private:
			Uniform threshold;
		};

		IMPLEMENT_SHADER_TYPE(RayTraceBloomThresholdFS, "raytrace/bloom_threshold.glsl", Fragment);

		class RayTraceBloomBlurFS : public Shader
		{
			SHADER_TYPE(RayTraceBloomBlurFS)

			RayTraceBloomBlurFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
			{
				Bind(direction, "direction");
			}

			void SetDirection(int axis)
			{
				glm::vec2 dir;
				dir[axis] = 1.0f;
				Set(direction, dir);
			}

		private:
			Uniform direction;
		};

		IMPLEMENT_SHADER_TYPE(RayTraceBloomBlurFS, "raytrace/bloom_blur.glsl", Fragment);

		class RayTraceBloomCombineFS : public Shader
		{
			SHADER_TYPE(RayTraceBloomCombineFS)
			using Shader::Shader;
		};

		IMPLEMENT_SHADER_TYPE(RayTraceBloomCombineFS, "raytrace/bloom_combine.glsl", Fragment);

		class RayTraceExposureScaleFS : public Shader
		{
			SHADER_TYPE(RayTraceExposureScaleFS)

			RayTraceExposureScaleFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
			{
				Bind(exposure, "exposure");
			}

			void SetExposure(float newExposure)
			{
				Set(exposure, newExposure);
			}

		private:
			Uniform exposure;
		};

		IMPLEMENT_SHADER_TYPE(RayTraceExposureScaleFS, "raytrace/exposure_scale.glsl", Fragment);

		class RayTraceTonemapFS : public Shader
		{
			SHADER_TYPE(RayTraceTonemapFS)
			using Shader::Shader;
		};

		IMPLEMENT_SHADER_TYPE(RayTraceTonemapFS, "raytrace/tonemap.glsl", Fragment);


		RaytracedRenderer::RaytracedRenderer(Game *game, Renderer *renderer)
			: game(game), renderer(renderer)
		{

		}

		RaytracedRenderer::~RaytracedRenderer()
		{
			Disable();
		}

		RenderTarget::Ref Tonemap(Renderer *r, RenderTarget::Ref input)
		{
			// Highpass luminance.
			auto desc = input->GetDesc();
			desc.format = PF_SRGB8_A8;
			auto output = r->RTPool->Get(desc);
			r->SetRenderTarget(output, nullptr);
			r->ShaderControl->BindPipeline<BasicPostVS, RayTraceTonemapFS>(r->GlobalShaders);

			input->GetTexture().Bind(0);
			DrawScreenCover();

			return output;
		}

		RenderTarget::Ref Bloom(Renderer *r, RenderTarget::Ref input)
		{
			// Highpass luminance.
			auto bloomThresholdTarget = r->RTPool->Get(input->GetDesc());
			r->SetRenderTarget(bloomThresholdTarget, nullptr);
			r->GlobalShaders->Get<RayTraceBloomThresholdFS>()->UpdateThreshold(CVarBloomThreshold.Get());
			r->ShaderControl->BindPipeline<BasicPostVS, RayTraceBloomThresholdFS>(r->GlobalShaders);

			input->GetTexture().Bind(0);
			DrawScreenCover();

			// Blur highpassed result.
			auto bloomBlurTarget1 = r->RTPool->Get(input->GetDesc());
			r->SetRenderTarget(bloomBlurTarget1, nullptr);
			r->GlobalShaders->Get<RayTraceBloomBlurFS>()->SetDirection(0);
			r->ShaderControl->BindPipeline<BasicPostVS, RayTraceBloomBlurFS>(r->GlobalShaders);

			bloomThresholdTarget->GetTexture().Bind(0);
			DrawScreenCover();

			bloomThresholdTarget.reset();
			auto bloomBlurTarget2 = r->RTPool->Get(input->GetDesc());
			r->SetRenderTarget(bloomBlurTarget2, nullptr);
			r->GlobalShaders->Get<RayTraceBloomBlurFS>()->SetDirection(1);
			r->ShaderControl->BindPipeline<BasicPostVS, RayTraceBloomBlurFS>(r->GlobalShaders);

			bloomBlurTarget1->GetTexture().Bind(0);
			DrawScreenCover();

			// Combine.
			auto outputTarget = r->RTPool->Get(input->GetDesc());
			r->SetRenderTarget(outputTarget, nullptr);
			r->ShaderControl->BindPipeline<BasicPostVS, RayTraceBloomCombineFS>(r->GlobalShaders);

			input->GetTexture().Bind(0);
			bloomBlurTarget2->GetTexture().Bind(1);
			DrawScreenCover();

			return outputTarget;
		}

		float autoEV100(float lum)
		{
			// Lagarde/Rousiers 2014
			return std::log2(lum * 100.0 / 12.5);
		}

		float manualEV100(float aperture, float shutterTime, float iso)
		{
			// Lagarde/Rousiers 2014
			return std::log2((aperture * aperture / shutterTime) * (100.0 / iso));
		}

		float Exposure(Renderer *r, RenderTarget::Ref input, RenderTarget::Ref &output, float lastEV100)
		{
			float exposureScale = 1.0f, ev100 = lastEV100;

			//glm::vec4 pixel0;
			//glGetTextureSubImage(input->GetTexture().handle, 0, 256, 256, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(pixel0), &pixel0);
			//std::cerr << pixel0[3] << std::endl;

			if (CVarExposure.Get() == 0.0f)
			{
				auto desc = input->GetDesc();
				desc.levels = Texture::FullyMipmap;
				desc.format = PF_RGBA16F;
				auto x = r->RTPool->Get(desc);
				auto downsampleTarget = r->RTPool->Get(desc);

				r->SetRenderTarget(downsampleTarget, nullptr);
				r->GlobalShaders->Get<RayTraceExposureScaleFS>()->SetExposure(1.0f);
				r->ShaderControl->BindPipeline<BasicPostVS, RayTraceExposureScaleFS>(r->GlobalShaders);

				input->GetTexture().Bind(0);
				DrawScreenCover();

				auto &tex = downsampleTarget->GetTexture();
				glGenerateTextureMipmap(tex.handle);

				glm::vec4 pixel;
				glGetTextureSubImage(tex.handle, tex.levels - 1, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(pixel), &pixel);

				const glm::vec4 digitalLumCoeff(0.299, 0.587, 0.114, 0.0);
				float lum = glm::dot(pixel, digitalLumCoeff);
				ev100 = autoEV100(lum);

				if (std::isnan(ev100))
				{
					ev100 = lastEV100;
				}

				// EWMA
				ev100 = lastEV100 * 0.9 + ev100 * 0.1;

				float minEV100 = manualEV100(1.4, 0.1, 100);
				float maxEV100 = manualEV100(16, 0.01, 100);
				//Logf("%f %f %f / %f, %f %f", pixel[0], pixel[1], pixel[2], ev100, minEV100, maxEV100);

				if (ev100 < minEV100)
					ev100 = minEV100;

				if (ev100 > maxEV100)
					ev100 = maxEV100;

				// Lagarde/Rousiers 2014
				exposureScale = 1.0 / (1.2 * std::pow(2.0, ev100));
				//Logf("%f", exposureScale);
				//Logf("%f %f %f", pixel[0], pixel[1], pixel[2]);
				//pixel *= exposureScale;
				//Logf("%f %f %f", pixel[0], pixel[1], pixel[2]);
			}
			else
			{
				exposureScale = CVarExposure.Get();
			}

			r->SetRenderTarget(output, nullptr);
			r->GlobalShaders->Get<RayTraceExposureScaleFS>()->SetExposure(exposureScale);
			r->ShaderControl->BindPipeline<BasicPostVS, RayTraceExposureScaleFS>(r->GlobalShaders);

			input->GetTexture().Bind(0);
			DrawScreenCover();

			return ev100;
		}

		void RaytracedRenderer::Render()
		{
			const int wgsize = 16;
			auto image = target->GetTexture();

			glm::vec3 eyePos = glm::vec3(view.invViewMat * glm::vec4(0, 0, 0, 1));
			glm::mat3 eyeRot = glm::transpose(glm::mat3(view.viewMat));

			glDisable(GL_STENCIL_TEST);
			glDisable(GL_SCISSOR_TEST);

			if (!CVarPause.Get())
			{
				glm::uvec2 invocationSize(CVarInvocationSize.Get());

				if (invocationSize.x == 0)
					invocationSize.x = view.extents.x;

				if (invocationSize.y == 0)
					invocationSize.y = view.extents.y;

				invocationOffset.x += invocationSize.x;
				if (invocationOffset.x >= (uint32) view.extents.x)
				{
					invocationOffset.y += invocationSize.y;
					invocationOffset.x = 0;
				}

				if (invocationOffset.y >= (uint32) view.extents.y)
					invocationOffset.y = 0;

				// Call path tracer.
				RenderPhase phase("PathTraceSceneCS", renderer->Timer);

				auto shader = renderer->GlobalShaders->Get<PathTraceSceneCS>();
				shader->UpdateInvocation(invocationOffset);
				shader->UpdateParameters(view.fov, eyePos, eyeRot);
				renderer->ShaderControl->BindPipeline<PathTraceSceneCS>(renderer->GlobalShaders);

				image.BindImage(0, GL_READ_WRITE);
				baseColorRoughnessAtlas.Bind(0);
				normalMetalnessAtlas.Bind(1);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				glDispatchCompute((invocationSize.x + wgsize - 1) / wgsize, (invocationSize.y + wgsize - 1) / wgsize, 1);
				glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
			}

			{
				RenderPhase phase("RayTracerPostProcess", renderer->Timer);

				auto outDesc = target->GetDesc();
				outDesc.format = PF_RGBA16F;
				auto output = renderer->RTPool->Get(outDesc);

				lastEV100 = Exposure(renderer, target, output, lastEV100);

				if (CVarBloom.Get())
					output = Bloom(renderer, output);

				if (CVarTonemap.Get())
					output = Tonemap(renderer, output);

				// Draw output to viewport.
				renderer->SetDefaultRenderTarget();
				glEnable(GL_SCISSOR_TEST);
				//view.offset.x = view.extents.x / 2;
				renderer->PrepareForView(view);
				glViewport(0, 0, view.extents.x, view.extents.y);
				renderer->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(renderer->GlobalShaders);

				output->GetTexture().Bind(0);
				DrawScreenCover();
			}
		}

		bool RaytracedRenderer::Enable(ecs::View newView)
		{
			bool forceClear = false;

			if (!enabled)
			{
				enabled = true;
				forceClear = true;

				target = renderer->RTPool->Get({ PF_RGBA32F, newView.extents });

				gpuSceneCtx = new GPUSceneContext;
				ResetMaterialCache(*gpuSceneCtx);
				ResetGeometryCache(*gpuSceneCtx);
				view = newView;
			}

			if (!cacheUpdating && gpuSceneCtx)
			{
				// Cache update is complete
				renderer->GlobalShaders->Get<PathTraceSceneCS>()->UpdateSceneData(*gpuSceneCtx);
				delete gpuSceneCtx;
				gpuSceneCtx = nullptr;
			}

			if (CVarDynamicView.Get() && view.viewMat != newView.viewMat && enabled)
			{
				view = newView;
				forceClear = true;
			}

			if (forceClear)
			{
				renderer->SetRenderTarget(target, nullptr);
				glDisable(GL_SCISSOR_TEST);
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT);
			}

			// Disable rendering while the cache is updating in the background.
			return !cacheUpdating;
		}

		void RaytracedRenderer::Disable()
		{
			if (!enabled) return;

			enabled = false;
			baseColorRoughnessAtlas.Delete();
			normalMetalnessAtlas.Delete();
			target.reset();
		}

		void RaytracedRenderer::ResetMaterialCache(GPUSceneContext &ctx)
		{
			int maxSize = 1;
			int texIndex = 0;

			for (auto ent : game->entityManager.EntitiesWith<ecs::Renderable>())
			{
				auto comp = ent.Get<ecs::Renderable>();
				// TODO(?): Use a more generic interface of some sort, Not all renderables have a scene.
				tinygltf::Scene *scene = NULL;//comp->model->GetScene();
				for (auto &it : scene->materials)
				{
					auto material = it.second;
					auto key = comp->model->name + it.first;

					if (ctx.materials.find(key) == ctx.materials.end())
					{
						auto baseColorName = material.values["diffuse"].string_value;
						if (baseColorName == "")
							continue;

						MaterialInfo info;
						info.baseColorTex = &scene->textures[baseColorName];
						info.baseColorImg = &scene->images[info.baseColorTex->source];

						auto roughnessName = material.values["specular"].string_value;
						if (roughnessName != "")
						{
							info.roughnessTex = &scene->textures[roughnessName];
							info.roughnessImg = &scene->images[info.roughnessTex->source];
							info.roughnessInverted = true;
						}

						roughnessName = material.values["roughness"].string_value;
						if (roughnessName != "")
						{
							info.roughnessTex = &scene->textures[roughnessName];
							info.roughnessImg = &scene->images[info.roughnessTex->source];
							info.roughnessInverted = false;
						}
						else
						{
							auto roughnessVal = material.values["roughness"].number_array;
							if (roughnessVal.size())
							{
								info.roughness = roughnessVal[0];
							}
						}

						auto metalnessName = material.values["metal"].string_value;
						if (metalnessName != "")
						{
							info.metalnessTex = &scene->textures[metalnessName];
							info.metalnessImg = &scene->images[info.metalnessTex->source];
						}
						else
						{
							auto metalnessVal = material.values["metal"].number_array;
							if (metalnessVal.size())
							{
								info.metalness = metalnessVal[0];
							}
						}

						auto normalName = material.values["normal"].string_value;
						if (normalName != "")
						{
							info.normalTex = &scene->textures[normalName];
							info.normalImg = &scene->images[info.normalTex->source];
						}

						auto f0 = material.values["f0"].number_array;
						for (size_t i = 0; i < 3 && i < f0.size(); i++)
						{
							info.f0[i] = f0[i];
						}

						ctx.materials[key] = info;

						if (info.baseColorImg->height > maxSize)
							maxSize = info.baseColorImg->height;

						if (info.baseColorImg->width > maxSize)
							maxSize = info.baseColorImg->width;
					}
				}
			}

			maxSize = CeilToPowerOfTwo(maxSize);

			baseColorRoughnessAtlas
			.Delete()
			.Create(GL_TEXTURE_2D_ARRAY)
			.Filter(GL_LINEAR, GL_LINEAR)
			.Wrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE)
			.Size(maxSize, maxSize, ctx.materials.size())
			.Storage(PF_SRGB8_A8);

			normalMetalnessAtlas
			.Delete()
			.Create(GL_TEXTURE_2D_ARRAY)
			.Filter(GL_LINEAR, GL_LINEAR)
			.Wrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE)
			.Size(maxSize, maxSize, ctx.materials.size())
			.Storage(PF_RGBA8);

			Assert(ctx.materials.size() <= MAX_MATERIALS, "reached max materials");
			ctx.matData.nMaterials = ctx.materials.size();

			for (auto &it : ctx.materials)
			{
				auto &mat = it.second;

				// BaseColor
				auto bcimg = mat.baseColorImg;
				auto bctex = mat.baseColorTex;
				uint8 *bcdata = bcimg->image.data();

				// Roughness
				auto rimg = mat.roughnessImg;
				auto rtex = mat.roughnessTex;
				uint8 *rdata = rimg ? rimg->image.data() : nullptr;

				// Metalness
				auto mimg = mat.metalnessImg;
				auto mtex = mat.metalnessTex;
				uint8 *mdata = mimg ? mimg->image.data() : nullptr;

				// Normals
				auto nimg = mat.normalImg;
				auto ntex = mat.normalTex;
				uint8 *ndata = nimg ? nimg->image.data() : nullptr;

				Assert(bctex->target == GL_TEXTURE_2D, "assertion failed");
				int bcstride = 0, rstride = 0, mstride = 0, nstride = 0;

				if (bctex->format == GL_RGBA)
				{
					Assert(bctex->internalFormat == GL_RGBA || bctex->internalFormat == GL_RGBA8, "assertion failed");
					bcstride = 4;
				}
				else
				{
					Assert(bctex->format == GL_RGB, "assertion failed");
					Assert(bctex->internalFormat == GL_RGB || bctex->internalFormat == GL_RGB8, "assertion failed");
					bcstride = 3;
				}

				if (rtex)
				{
					Assert(rtex->target == GL_TEXTURE_2D, "assertion failed");
					if (rtex->format == GL_RED)
						rstride = 1;
					else if (rtex->format == GL_RGB)
						rstride = 3;
					else if (rtex->format == GL_RGBA)
						rstride = 4;
				}

				if (mtex)
				{
					Assert(mtex->target == GL_TEXTURE_2D, "assertion failed");
					if (mtex->format == GL_RED)
						mstride = 1;
					else if (mtex->format == GL_RGB)
						mstride = 3;
					else if (mtex->format == GL_RGBA)
						mstride = 4;
				}

				if (ntex)
				{
					Assert(ntex->target == GL_TEXTURE_2D, "assertion failed");
					Assert(ntex->type == GL_UNSIGNED_BYTE, "assertion failed");
					if (ntex->format == GL_RGB)
						nstride = 3;
					else if (ntex->format == GL_RGBA)
						nstride = 4;
				}

				auto pixelCount = bcimg->width * bcimg->height;
				auto data = new uint8[pixelCount * 4];

				for (int bci = 0, ri = 0, di = 0; di < pixelCount * 4; bci += bcstride, ri += rstride, di += 4)
				{
					data[di + 0] = bcdata[bci + 0];
					data[di + 1] = bcdata[bci + 1];
					data[di + 2] = bcdata[bci + 2];

					if (rstride)
					{
						auto val = rdata[ri];
						if (mat.roughnessInverted)
						{
							val = 255 - val;

							//float srgb = ((float)val) / 255.0f;
							//float R = srgb * (srgb * (srgb * 0.305306011 + 0.682171111) + 0.012522878);
							//val = uint8(R * 255.0f);
						}
						data[di + 3] = val;
					}
					else
					{
						data[di + 3] = uint8(mat.roughness * 255.0f);
					}
				}

				baseColorRoughnessAtlas.Image3D(data, 0, bcimg->width, bcimg->height, 1, 0, 0, texIndex, false);

				for (int ni = 0, mi = 0, di = 0; di < pixelCount * 4; ni += nstride, mi += mstride, di += 4)
				{
					if (nstride)
					{
						data[di + 0] = ndata[ni + 0];
						data[di + 1] = ndata[ni + 1];
						data[di + 2] = ndata[ni + 2];
					}
					else
					{
						data[di + 0] = 128;
						data[di + 1] = 128;
						data[di + 2] = 255;
					}

					data[di + 3] = mstride ? mdata[mi] : uint8(mat.metalness * 255.0f);
				}

				normalMetalnessAtlas.Image3D(data, 0, bcimg->width, bcimg->height, 1, 0, 0, texIndex, false);
				delete[] data;

				auto &matRef = ctx.matData.materials[texIndex];
				matRef.baseColorRoughnessIdx = texIndex;
				matRef.baseColorRoughnessSize.x = (float) bcimg->width / (float) maxSize;
				matRef.baseColorRoughnessSize.y = (float) bcimg->height / (float) maxSize;

				if (nstride || mstride)
				{
					matRef.normalMetalnessIdx = texIndex;
					matRef.normalMetalnessSize.x = (float) bcimg->width / (float) maxSize;
					matRef.normalMetalnessSize.y = (float) bcimg->height / (float) maxSize;
				}

				matRef.f0 = mat.f0;

				mat.id = texIndex;
				texIndex++;
			}
		}

		void RaytracedRenderer::ResetGeometryCache(GPUSceneContext &ctx)
		{
			cacheUpdating = true;

			vector<std::pair<shared_ptr<Model>, glm::mat4>> models;

			for (auto ent : game->entityManager.EntitiesWith<ecs::Renderable, ecs::Transform>())
			{
				auto renderable = ent.Get<ecs::Renderable>();
				auto transform = ent.Get<ecs::Transform>();
				auto trmat = transform->GetGlobalTransform(game->entityManager);

				models.push_back(std::make_pair(renderable->model, trmat));
			}

			std::thread updater([ &, models]
			{
				for (auto &it : models)
				{
					auto model = it.first;
					auto trmat = it.second;

					for (auto primitive : model->primitives)
					{
						ctx.AppendPrimitive(trmat, model.get(), primitive);
					}
				}

				cacheUpdating = false;
			});

			updater.detach();
		}
	}
}
