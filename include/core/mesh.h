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

#ifndef MESH_H
#define MESH_H

#include "core/maths.h"
#include "core/render.h"
#include "core/transform-manager.h"

namespace bkk
{
  namespace core
  {
    namespace mesh
    {
      struct aabb_t
      {
        maths::vec3 min_;
        maths::vec3 max_;
      };

      struct skeleton_t
      {
        transform_manager_t txManager_;

        handle_t* bones_;
        maths::mat4* bindPose_;

        maths::mat4 rootBoneInverseTransform_;

        u32 boneCount_;
        u32 nodeCount_;
      };

      struct bone_transform_t
      {
        maths::vec3 position_;
        maths::vec3 scale_;
        maths::quat orientation_;
      };

      struct skeletal_animation_t
      {
        u32 frameCount_;
        u32 nodeCount_;
        f32 duration_;  //In ms

        handle_t* nodes_;    //Handles of animated nodes
        bone_transform_t* data_;
      };


      struct skeletal_animator_t
      {
        f32 cursor_;
        float speed_;

        skeleton_t* skeleton_;
        const skeletal_animation_t* animation_;

        maths::mat4* boneTransform_;      //Final bones transforms for current time in the animation
        render::gpu_buffer_t buffer_;    //Uniform buffer with the final transformation of each bone
      };

      struct mesh_t
      {
        render::gpu_buffer_t vertexBuffer_;
        render::gpu_buffer_t indexBuffer_;

        u32 vertexCount_;
        u32 indexCount_;
        aabb_t aabb_;

        //Only used for skinned meshes
        skeleton_t* skeleton_ = nullptr;
        skeletal_animation_t* animations_ = nullptr;
        u32 animationCount_ = 0u;

        render::vertex_format_t vertexFormat_;
      };

      struct material_t
      {
        maths::vec3 kd_;
        maths::vec3 ks_;

        char diffuseMap_[128];
      };

      enum export_flags_e
      {
        EXPORT_POSITION_ONLY = 0,
        EXPORT_NORMALS = 1,
        EXPORT_UV = 2,
        EXPORT_BONE_WEIGHTS = 4,
        EXPORT_ALL = EXPORT_NORMALS | EXPORT_UV | EXPORT_BONE_WEIGHTS

      };

      ///Mesh API

      void create(const render::context_t& context,
        const uint32_t* indexData, uint32_t indexDataSize,
        const void* vertexData, size_t vertexDataSize,
        render::vertex_attribute_t* attribute, uint32_t attributeCount,
        render::gpu_memory_allocator_t* allocator, mesh_t* mesh);


      //Load all submeshes from a file
      //Warning: Allocates an array of meshes from the heap (returned by reference in 'meshes') and passes ownership of that memory to the caller
      uint32_t createFromFile(const render::context_t& context, const char* file, export_flags_e exportFlags, render::gpu_memory_allocator_t* allocator, mesh_t** meshes);

      //Load a single submesh from a file
      void createFromFile(const render::context_t& context, const char* file, export_flags_e exportFlags, render::gpu_memory_allocator_t* allocator, uint32_t subMesh, mesh_t* mesh);

      uint32_t loadMaterials(const char* file, uint32_t** materialIndices, material_t** materials);

      void draw(render::command_buffer_t commandBuffer, const mesh_t& mesh);
      void drawInstanced(render::command_buffer_t commandBuffer, u32 instanceCount, render::gpu_buffer_t* instanceBuffer, u32 instancedAttributesCount, const mesh_t& mesh);
      void destroy(const render::context_t& context, mesh_t* mesh, render::gpu_memory_allocator_t* allocator = nullptr);

      //Animator
      void animatorCreate(const render::context_t& context, const mesh_t& mesh, u32 animationIndex, float speedFactor, skeletal_animator_t* animator);
      void animatorUpdate(const render::context_t& context, f32 deltaTimeInMs, skeletal_animator_t* animator);
      void animatorDestroy(const render::context_t& context, skeletal_animator_t* animator);

      mesh_t fullScreenQuad(const render::context_t& context);
      mesh_t unitQuad(const render::context_t& context);
      mesh_t unitCube(const render::context_t& context);

    } //mesh namespace
  }//core namespace
}//namespace bkk
#endif  /*  MESH_H   */