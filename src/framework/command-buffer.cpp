
#include "core/mesh.h"

#include "framework/command-buffer.h"
#include "framework/frame-buffer.h"
#include "framework/renderer.h"
#include "framework/camera.h"


using namespace bkk;
using namespace bkk::core;
using namespace bkk::framework;


command_buffer_t::command_buffer_t()
{}

command_buffer_t::command_buffer_t(const command_buffer_t& cmdBuffer)
  :renderer_(cmdBuffer.renderer_),
  frameBuffer_(cmdBuffer.frameBuffer_),
  commandBuffer_(cmdBuffer.commandBuffer_),
  semaphore_(cmdBuffer.semaphore_),
  clearColor_(cmdBuffer.clearColor_),
  clear_(cmdBuffer.clear_),
  released_(cmdBuffer.released_)
{

}

command_buffer_t::command_buffer_t(renderer_t* renderer, frame_buffer_handle_t frameBuffer, command_buffer_t* prevCommandBuffer)
:renderer_(renderer),
 frameBuffer_(frameBuffer),
 clearColor_(0.0f, 0.0f, 0.0f, 0.0f),
 clear_(false),
 released_(false)
{ 
  VkSemaphore* waitSemaphore = nullptr;
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  if (prevCommandBuffer != nullptr)
  {
    waitSemaphore = prevCommandBuffer->getSemaphore();
  }
  
  semaphore_ = bkk::core::render::semaphoreCreate(renderer->getContext());
  VkSemaphore* signalSemaphore = &semaphore_;
  if (frameBuffer == core::NULL_HANDLE )
  {
    //Rendering to back buffer
    signalSemaphore = renderer->getRenderCompleteSemaphore();
    frameBuffer_ = renderer_->getBackBuffer();
  }
  
  render::commandBufferCreate(renderer->getContext(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, waitSemaphore, &waitStage, waitSemaphore == nullptr ? 0 : 1, signalSemaphore, 1u, render::command_buffer_t::GRAPHICS, &commandBuffer_);
}

command_buffer_t::~command_buffer_t()
{}

void command_buffer_t::clearRenderTargets(core::maths::vec4 color)
{
  clear_ = true;
  clearColor_ = color;
}

void command_buffer_t::beginCommandBuffer()
{  
  frame_buffer_t* frameBuffer = renderer_->getFrameBuffer(frameBuffer_);

  render::context_t& context = renderer_->getContext();
  render::commandBufferBegin(context, commandBuffer_);
    
  VkClearValue* clearValues = nullptr;
  uint32_t clearValuesCount = 0u;
  if (clear_)
  {
    clearValuesCount = frameBuffer->getTargetCount() + 1;
    clearValues = new VkClearValue[clearValuesCount];
    for (uint32_t i(0); i < clearValuesCount-1; ++i)
      clearValues[i].color = { { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w } };

    clearValues[clearValuesCount-1].depthStencil = { 1.0f,0 };
  }

  render::commandBufferRenderPassBegin(context, &frameBuffer->getFrameBuffer(), &clearValues[0], clearValuesCount, commandBuffer_);
  
  if (clearValues)
    delete[] clearValues;
}

void command_buffer_t::render(actor_t* actors, uint32_t actorCount, const char* passName)
{
  camera_t* camera = renderer_->getActiveCamera();

  beginCommandBuffer();
  
  
  for (uint32_t i = 0; i < actorCount; ++i)
  {
    material_t* material = renderer_->getMaterial(actors[i].getMaterial());
    core::mesh::mesh_t* mesh = renderer_->getMesh(actors[i].getMesh());

    if (material && mesh )
    {
      core::render::graphics_pipeline_t pipeline = material->getPipeline(passName, frameBuffer_, renderer_);
      if (pipeline.handle_ != VK_NULL_HANDLE)
      {
        //TODO: Order objects by material and bind pipeline and camera ubo only once for all objects
        //sharing the same material
        render::graphicsPipelineBind(commandBuffer_, pipeline );

        //Camera uniform buffer
        render::descriptorSetBind(commandBuffer_, pipeline.layout_, 0, &camera->descriptorSet_, 1u);
      
        //Object uniform buffer
        render::descriptorSetBind(commandBuffer_, pipeline.layout_, 1, &actors[i].descriptorSet_, 1u);

        //Material descriptor set
        render::descriptor_set_t materialDescriptorSet = material->getDescriptorSet(passName);
        render::descriptorSetBind(commandBuffer_, pipeline.layout_, 2, &materialDescriptorSet, 1u);

        //Draw call
        core::mesh::draw(commandBuffer_, *mesh);
      }
    }
  }
  
  render::commandBufferRenderPassEnd(commandBuffer_);
  render::commandBufferEnd(commandBuffer_);
}

void command_buffer_t::blit(render_target_handle_t renderTarget, material_handle_t materialHandle, const char* pass)
{
  material_t* material = renderer_->getTextureBlitMaterial();
  if (materialHandle != NULL_HANDLE)
  {    
    material = renderer_->getMaterial(materialHandle);
  }

  if (!material) return;

  if (renderTarget != core::NULL_HANDLE)
  {
    render::texture_t texture = renderer_->getRenderTarget(renderTarget)->getColorBuffer();
    //render::textureChangeLayoutNow(renderer_->getContext(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &texture);
    material->setTexture("MainTexture", texture);
  }

  camera_t* camera = renderer_->getActiveCamera();
  actor_t* actor = renderer_->getActor( renderer_->getRootActor() );
  mesh::mesh_t* mesh = renderer_->getMesh(actor->getMesh() );

  const char* passName = "blit";
  if (pass != nullptr)
    passName = pass;

  core::render::graphics_pipeline_t pipeline = material->getPipeline(passName, frameBuffer_, renderer_);  
  render::descriptor_set_t materialDescriptorSet = material->getDescriptorSet(passName);

  beginCommandBuffer();

  render::graphicsPipelineBind(commandBuffer_, pipeline);
  render::descriptorSetBind(commandBuffer_, pipeline.layout_, 0, &camera->descriptorSet_, 1u);
  render::descriptorSetBind(commandBuffer_, pipeline.layout_, 1, &actor->descriptorSet_, 1u);
  render::descriptorSetBind(commandBuffer_, pipeline.layout_, 2, &materialDescriptorSet, 1u);

  core::mesh::draw(commandBuffer_, *mesh);

  render::commandBufferRenderPassEnd(commandBuffer_);
  render::commandBufferEnd(commandBuffer_);
}

void command_buffer_t::submit()
{
  render::context_t& context = renderer_->getContext();  
  render::commandBufferSubmit(context, commandBuffer_);
}

void command_buffer_t::release()
{
  if (!released_)
  {    
    renderer_->releaseCommandBuffer(this);
    released_ = true;
  }
}

void command_buffer_t::cleanup()
{
  if (commandBuffer_.handle_ != VK_NULL_HANDLE)
  {
    render::context_t& context = renderer_->getContext();
    core::render::commandBufferDestroy(context, &commandBuffer_);
    commandBuffer_ = {};
    render::semaphoreDestroy(context, semaphore_);
    released_ = true;
  }
}

VkSemaphore* command_buffer_t::getSemaphore() 
{ 
  if( frameBuffer_ == renderer_->getBackBuffer() )
    return renderer_->getRenderCompleteSemaphore();

  return &semaphore_; 
}