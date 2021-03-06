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

#include "core/maths.h"
#include "core/packed-freelist.h"

#include "framework/camera.h"
#include "framework/actor.h"
#include "framework/renderer.h"

using namespace bkk::core;
using namespace bkk::framework;

camera_t::camera_t()
{}

camera_t::camera_t(projection_mode_e projectionMode, float fov, float aspect, float nearPlane, float farPlane)
:projection_(projectionMode),
fov_(fov),
aspect_(aspect),
nearPlane_(nearPlane),
farPlane_(farPlane)
{}

void camera_t::update(renderer_t* renderer)
{
  if (projection_ == camera_t::PERSPECTIVE_PROJECTION)
  {
    uniforms_.projection_ = maths::perspectiveProjectionMatrix(fov_, aspect_, nearPlane_, farPlane_);
  }
  else
  {
    uniforms_.projection_ = maths::orthographicProjectionMatrix(-fov_, fov_, fov_, -fov_, nearPlane_, farPlane_);
  }

  maths::invertMatrix(uniforms_.projection_, uniforms_.projectionInverse_);
  maths::invertMatrix(uniforms_.viewToWorld_, uniforms_.worldToView_);

  render::context_t& context = renderer->getContext();
  if (uniformBuffer_.handle_ == VK_NULL_HANDLE)
  {
    //Create buffer
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      (void*)&uniforms_, sizeof(uniforms_),
      nullptr, &uniformBuffer_);

    render::descriptor_t descriptor = render::getDescriptor(uniformBuffer_);
    render::descriptorSetCreate(context, renderer->getDescriptorPool(), renderer->getGlobalsDescriptorSetLayout(), &descriptor, &descriptorSet_);
  }
  else
  {
    render::gpuBufferUpdate(context, (void*)&uniforms_,
      0u, sizeof(uniforms_),
      &uniformBuffer_);
  }
}

void camera_t::cull(actor_t* actors, uint32_t actorCount)
{
  if (visibleActors_ != nullptr)
  {
    delete[] visibleActors_;
  }

  visibleActorsCount_ = 0u;
  visibleActors_ = new actor_t[actorCount];
  for (uint32_t i = 0; i < actorCount; ++i)
  {
    visibleActors_[i] = actors[i];
    visibleActorsCount_++;
  }
}

void camera_t::destroy(renderer_t* renderer)
{
  render::context_t& context = renderer->getContext();
  if (uniformBuffer_.handle_ != VK_NULL_HANDLE)
  {
    render::gpuBufferDestroy(context, nullptr, &uniformBuffer_);
    render::descriptorSetDestroy(context, &descriptorSet_);
  }
}

orbiting_camera_t::orbiting_camera_t()
:target_(0.0f,0.0f,0.0f),
 offset_(0.0f),
 angle_(maths::vec2(0.0f, 0.0f)),
 rotationSensitivity_(0.01f)
{
  Update();
}

orbiting_camera_t::orbiting_camera_t(const maths::vec3& target, const f32 offset, const maths::vec2& angle, f32 rotationSensitivity)
:target_(target), 
 offset_(offset),
 angle_(angle),
 rotationSensitivity_(rotationSensitivity)
{
  Update();
}

void orbiting_camera_t::Move(f32 amount)
{
  offset_ += amount;
  offset_ = maths::clamp(0.0f, offset_, offset_);

  Update();
}

void orbiting_camera_t::Rotate(f32 angleY, f32 angleZ)
{
  angle_.x = angle_.x + angleY * rotationSensitivity_;
  angle_.y = angle_.y + angleZ * rotationSensitivity_;
  Update();
}

void orbiting_camera_t::Update()
{
  maths::quat orientation = maths::quaternionFromAxisAngle(maths::vec3(1.0f, 0.0f, 0.0f), angle_.y) *
  maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), angle_.x);

  maths::mat4 tx = maths::createTransform(maths::vec3(0.0f, 0.0f, offset_), maths::VEC3_ONE, maths::QUAT_UNIT) * maths::createTransform(maths::VEC3_ZERO, maths::VEC3_ONE, orientation) * maths::createTransform(target_, maths::VEC3_ONE, maths::QUAT_UNIT);
  maths::invertMatrix(tx, view_);
}


free_camera_t::free_camera_t()
:tx_(),
 view_(),
 position_(0.0f, 0.0f, 0.0f),
 angle_(0.0f, 0.0f),
 velocity_(1.0f),
 rotationSensitivity_(0.01f),
 cameraHandle_(bkk::core::NULL_HANDLE),
 renderer_(nullptr)
{
  Update();
}

free_camera_t::free_camera_t(const maths::vec3& position, const maths::vec2& angle, f32 velocity, f32 rotationSensitivity)
  :position_(position),
  angle_(angle),
  velocity_(velocity),
  rotationSensitivity_(rotationSensitivity),
  cameraHandle_(bkk::core::NULL_HANDLE),
  renderer_(nullptr)
{
  Update();
}

void free_camera_t::Move(f32 xAmount, f32 zAmount)
{
  position_ = position_ + (zAmount * velocity_ * tx_.row(2).xyz()) + (xAmount * velocity_ * tx_.row(0).xyz());
  Update();
}

void free_camera_t::Rotate(f32 angleY, f32 angleX)
{
  angle_.y = angle_.y + angleY * rotationSensitivity_;
  angleX = angle_.x + angleX * rotationSensitivity_;
  if (angleX < PI_2 && angleX > -PI_2)
  {
    angle_.x = angleX;
  }

  Update();
}

void free_camera_t::Update()
{
  maths::quat orientation = maths::quaternionFromAxisAngle(maths::vec3(1.0f, 0.0f, 0.0f), angle_.x) * maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), angle_.y);
  tx_ = maths::createTransform(position_, maths::VEC3_ONE, orientation);
  maths::invertMatrix(tx_, view_);

  if (cameraHandle_ != bkk::core::NULL_HANDLE)
  {
    camera_t* camera = renderer_->getCamera(cameraHandle_);
    if (camera)
    {
      camera->uniforms_.viewToWorld_ = tx_;
      camera->uniforms_.worldToView_ = view_;
    }
  }
}

void free_camera_t::setCameraHandle(camera_handle_t cameraHandle, renderer_t* renderer)
{
  cameraHandle_ = cameraHandle;
  renderer_ = renderer;
  Update();
}