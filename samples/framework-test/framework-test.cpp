/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "core/mesh.h"
#include "core/maths.h"
#include "core/image.h"

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/command-buffer.h"

using namespace bkk::core;
using namespace bkk::framework;

class framework_test_t : public application_t
{
private:
  struct light_t
  {
    maths::vec4 position;
    maths::vec3 color;
    float radius;
  };

public:
  framework_test_t()
  :application_t("Framework test", 1200u, 800u, 3u),
   cameraController_(maths::vec3(0.0f, 4.0f, 12.0f), maths::vec2(0.1f, 0.0f), 1.0f, 0.01f),
   bloomEnabled_(true),
   bloomTreshold_(1.0f),
   lightIntensity_(1.0f),
   exposure_(1.5f)
  {
    maths::uvec2 imageSize(1200u, 800u);

    //create scene framebuffer
    sceneRT_ = renderer_.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    sceneFBO_ = renderer_.frameBufferCreate(&sceneRT_, 1u);

    //create light buffer
    lightBuffer_ = createLightBuffer();

    //Load environment map
    image::image2D_t cubemapImage = {};
    image::load("../resources/Circus_Backstage_3k.hdr", true, &cubemapImage);
    render::textureCubemapCreateFromEquirectangularImage(getRenderContext(), cubemapImage, 2046u, true, &skybox_);
    render::diffuseConvolution(getRenderContext(), skybox_, 64u, &irradianceMap_);
    render::specularConvolution(getRenderContext(), skybox_, 256u, 4u, &specularMap_);
    render::brdfConvolution(getRenderContext(), 512u, &brdfLut_);

    shader_handle_t skyboxShader = renderer_.shaderCreate("../../shaders/sky-box.shader");
    skyboxMaterial_ = renderer_.materialCreate(skyboxShader);
    renderer_.getMaterial(skyboxMaterial_)->setTexture("CubeMap", skybox_);

    //create meshes
    mesh::mesh_t teapotMesh;
    mesh::createFromFile(getRenderContext(), "../resources/teapot.obj", mesh::EXPORT_ALL, nullptr, 0, &teapotMesh);
    mesh_handle_t teapot = renderer_.addMesh(teapotMesh);
    mesh_handle_t plane = renderer_.addMesh(mesh::unitQuad(getRenderContext()));

    //create materials
    shader_handle_t shader = renderer_.shaderCreate("../framework-test/pbr.shader");
    material_handle_t material0 = renderer_.materialCreate(shader);
    material_t* materialPtr = renderer_.getMaterial(material0);
    materialPtr->setProperty("globals.albedo", maths::vec3(0.1f, 0.1f, 0.1f));
    materialPtr->setProperty("globals.F0", maths::vec3(0.9f, 0.9f, 0.9f));
    materialPtr->setProperty("globals.roughness", 0.15f);
    materialPtr->setProperty("globals.metallic", 0.8f);
    materialPtr->setTexture("irradianceMap", irradianceMap_);
    materialPtr->setTexture("specularMap", specularMap_);
    materialPtr->setTexture("brdfLUT", brdfLut_);
    materialPtr->setBuffer("lights", lightBuffer_);

    material_handle_t material1 = renderer_.materialCreate(shader);
    materialPtr = renderer_.getMaterial(material1);
    materialPtr->setProperty("globals.albedo", maths::vec3(0.5f, 0.5f, 0.5f));
    materialPtr->setProperty("globals.F0", maths::vec3(0.6f, 0.6f, 0.6f));
    materialPtr->setProperty("globals.roughness", 0.3f);
    materialPtr->setProperty("globals.metallic", 0.3f);
    materialPtr->setTexture("irradianceMap", irradianceMap_);
    materialPtr->setTexture("specularMap", specularMap_);
    materialPtr->setTexture("brdfLUT", brdfLut_);
    materialPtr->setBuffer("lights", lightBuffer_);

    material_handle_t material2 = renderer_.materialCreate(shader);
    materialPtr = renderer_.getMaterial(material2);
    materialPtr->setProperty("globals.albedo", maths::vec3(0.1f, 0.1f, 0.1f));
    materialPtr->setProperty("globals.F0", maths::vec3(0.0f, 0.0f, 0.0f));
    materialPtr->setProperty("globals.roughness", 1.0f);
    materialPtr->setProperty("globals.metallic", 0.0f);
    materialPtr->setTexture("irradianceMap", irradianceMap_);
    materialPtr->setTexture("specularMap", specularMap_);
    materialPtr->setTexture("brdfLUT", brdfLut_);
    materialPtr->setBuffer("lights", lightBuffer_);

    //create actors
    maths::mat4 transform = maths::createTransform(maths::vec3(-5.0f, -1.0f, 0.0f), maths::VEC3_ONE, maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), maths::degreeToRadian(30.0f)));
    renderer_.actorCreate("teapot0", teapot, material0, transform);

    transform = maths::createTransform(maths::vec3(5.0f, -1.0f, 0.0f), maths::VEC3_ONE, maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), maths::degreeToRadian(150.0f)));
    renderer_.actorCreate("teapot1", teapot, material1, transform);
    
    transform = maths::createTransform(maths::vec3(0.0f, -1.0f, 0.0f), maths::vec3(20.0f, 20.0f, 20.0f), maths::quaternionFromAxisAngle(maths::vec3(1, 0, 0), maths::degreeToRadian(90.0f)) );
    renderer_.actorCreate("plane", plane, material2, transform);
    
    //Bloom resources
    brightPixelsRT_ = renderer_.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    brightPixelsFBO_ = renderer_.frameBufferCreate(&brightPixelsRT_, 1u);
    blurVerticalRT_ = renderer_.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    blurVerticalFBO_ = renderer_.frameBufferCreate(&blurVerticalRT_, 1u);
    bloomRT_ = renderer_.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    bloomFBO_ = renderer_.frameBufferCreate(&bloomRT_, 1u);
    shader_handle_t bloomShader = renderer_.shaderCreate("../framework-test/bloom.shader");
    bloomMaterial_ = renderer_.materialCreate(bloomShader);
    shader_handle_t blendShader = renderer_.shaderCreate("../framework-test/blend.shader");
    blendMaterial_ = renderer_.materialCreate(blendShader);
    material_t* blendMaterial = renderer_.getMaterial(blendMaterial_);
    blendMaterial->setTexture("bloomBlur", renderer_.getRenderTarget(bloomRT_)->getColorBuffer());

    //create camera
    camera_ = renderer_.addCamera(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x/(float)imageSize.y, 0.1f, 100.0f));
    cameraController_.setCameraHandle(camera_, &renderer_);
  }
  
  render::gpu_buffer_t createLightBuffer()
  {    
    int lightCount = 2;
    std::vector<light_t> lights(lightCount);
    lights[0].position = maths::vec4(-7.0f, 5.0f, 0.0f, 1.0f);
    lights[0].color = maths::vec3(1.0f, 1.0f, 1.0f);
    lights[0].radius = 13.0f;
    lights[1].position = maths::vec4(7.0f, 5.0f, 0.0f, 1.0f);
    lights[1].color = maths::vec3(1.0f, 1.0f, 1.0f);
    lights[1].radius = 13.0f;

    //Create buffer
    render::gpu_buffer_t lightBuffer = {};
    render::context_t& context = getRenderContext();
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::STORAGE_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT, 
      nullptr, sizeof(light_t)*lightCount + sizeof(maths::vec4), nullptr,
      &lightBuffer );
    
    render::gpuBufferUpdate(context, &lightCount, 0u, sizeof(int), &lightBuffer);
    render::gpuBufferUpdate(context, &lightIntensity_, sizeof(int), sizeof(float), &lightBuffer);
    render::gpuBufferUpdate(context, lights.data(), sizeof(maths::vec4), lightCount * sizeof(light_t), &lightBuffer);

    return lightBuffer;
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    if (pressed)
    {
      float delta = 0.5f;
      switch (key)
      {
        case window::key_e::KEY_UP:
        case 'w':
          cameraController_.Move(0.0f, -delta);
          break;

        case window::key_e::KEY_DOWN:
        case 's':
          cameraController_.Move(0.0f, delta);
          break;

        case window::key_e::KEY_LEFT:
        case 'a':
          cameraController_.Move(-delta, 0.0f);
          break;

        case window::key_e::KEY_RIGHT:
        case 'd':
          cameraController_.Move(delta, 0.0f);
          break;

        default:
          break;
      }
    }
  }

  void onMouseMove(const maths::vec2& mousePos, const maths::vec2 &mouseDeltaPos)
  {
    if (getMousePressedButton() == window::MOUSE_RIGHT)
      cameraController_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);    
  }

  void onQuit() 
  {
    render::gpuBufferDestroy(getRenderContext(), nullptr, &lightBuffer_);
    render::textureDestroy(getRenderContext(), &skybox_);
    render::textureDestroy(getRenderContext(), &irradianceMap_);
    render::textureDestroy(getRenderContext(), &specularMap_);
    render::textureDestroy(getRenderContext(), &brdfLut_);
  }

  void render()
  {
    beginFrame();

    //Render scene
    renderer_.setupCamera(camera_);
    actor_t* visibleActors = nullptr;
    int count = renderer_.getVisibleActors(camera_, &visibleActors);

    command_buffer_t renderSceneCmd(&renderer_, sceneFBO_);
    renderSceneCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submit();
    renderSceneCmd.release();
    
    //Render skybox
    command_buffer_t renderSkyboxCmd = command_buffer_t(&renderer_, sceneFBO_, &renderSceneCmd);    
    renderSkyboxCmd.blit(bkk::core::NULL_HANDLE, skyboxMaterial_ );
    renderSkyboxCmd.submit();
    renderSkyboxCmd.release();
    
    if (bloomEnabled_)
    {
      material_t* bloomMaterial = renderer_.getMaterial(bloomMaterial_);
      bloomMaterial->setProperty("globals.bloomTreshold", bloomTreshold_);
      
      //Extract bright pixels from scene render target
      command_buffer_t extractBrightPixelsCmd = command_buffer_t(&renderer_, brightPixelsFBO_, &renderSkyboxCmd);
      extractBrightPixelsCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      extractBrightPixelsCmd.blit(sceneRT_ , bloomMaterial_, "extractBrightPixels" );
      extractBrightPixelsCmd.submit();
      extractBrightPixelsCmd.release();
      
      //Blur vertical pass
      command_buffer_t blurVerticalCmd = command_buffer_t(&renderer_, blurVerticalFBO_, &extractBrightPixelsCmd);
      blurVerticalCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blurVerticalCmd.blit(brightPixelsRT_, bloomMaterial_, "blurVertical");
      blurVerticalCmd.submit();
      blurVerticalCmd.release();

      //Blur horizontal pass
      command_buffer_t blurHorizontalCmd = command_buffer_t(&renderer_, bloomFBO_, &blurVerticalCmd);
      blurHorizontalCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blurHorizontalCmd.blit(blurVerticalRT_, bloomMaterial_, "blurHorizontal");
      blurHorizontalCmd.submit();
      blurHorizontalCmd.release();

      //Blend bloom and scene render targets
      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer_, bkk::core::NULL_HANDLE, &blurHorizontalCmd);
      blitToBackbufferCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blitToBackbufferCmd.blit(sceneRT_, blendMaterial_, "blend" );
      blitToBackbufferCmd.submit();
      blitToBackbufferCmd.release();
    }
    else
    {
      //Copy scene render target to the back buffer
      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer_);
      blitToBackbufferCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blitToBackbufferCmd.blit(sceneRT_, blendMaterial_);
      blitToBackbufferCmd.submit();
      blitToBackbufferCmd.release();
    }

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");
    ImGui::LabelText("", "General Settings");
    ImGui::SliderFloat("Light Intensity", &lightIntensity_, 0.0f, 10.0f);
    ImGui::SliderFloat("Exposure", &exposure_, 0.0f, 10.0f);

    ImGui::Separator();

    ImGui::LabelText("", "Bloom Settings");
    ImGui::Checkbox("Enable", &bloomEnabled_);
    ImGui::SliderFloat("Bloom Treshold", &bloomTreshold_, 0.0f, 10.0f);
    ImGui::End();

    //Set properties
    renderer_.getMaterial(blendMaterial_)->setProperty("globals.exposure", exposure_);
    render::gpuBufferUpdate(getRenderContext(), &lightIntensity_, sizeof(int), sizeof(float), &lightBuffer_);
  }

private:
  frame_buffer_handle_t sceneFBO_;
  render_target_handle_t sceneRT_;  
  render::gpu_buffer_t lightBuffer_;
  material_handle_t skyboxMaterial_;
  render::texture_t skybox_;
  render::texture_t irradianceMap_;
  render::texture_t specularMap_;
  render::texture_t brdfLut_;

  bool bloomEnabled_;
  material_handle_t bloomMaterial_;
  material_handle_t blendMaterial_;
  frame_buffer_handle_t bloomFBO_;
  render_target_handle_t bloomRT_;
  frame_buffer_handle_t blurVerticalFBO_;
  render_target_handle_t blurVerticalRT_;
  frame_buffer_handle_t brightPixelsRT_;
  render_target_handle_t brightPixelsFBO_;
  float bloomTreshold_;

  camera_handle_t camera_;
  free_camera_t cameraController_;

  float lightIntensity_;
  float exposure_;
};

int main()
{
  framework_test_t test;
  test.loop();

  return 0;
}