#include "comp_camera.h"

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <render/render.h>

#include "../resources/res_camera.h"
#include "gamesys_ddf.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    const uint8_t MAX_COUNT = 64;
    const uint8_t MAX_STACK_COUNT = 8;

    struct CameraWorld;

    struct CameraComponent
    {
        dmGameObject::HInstance m_Instance;
        CameraWorld* m_World;
        float m_AspectRatio;
        float m_Fov;
        float m_NearZ;
        float m_FarZ;
        uint32_t m_AutoAspectRatio : 1;
    };

    struct CameraWorld
    {
        dmArray<CameraComponent> m_Cameras;
        dmArray<CameraComponent*> m_FocusStack;
    };

    dmGameObject::CreateResult CompCameraNewWorld(void* context, void** world)
    {
        CameraWorld* cam_world = new CameraWorld();
        cam_world->m_Cameras.SetCapacity(MAX_COUNT);
        cam_world->m_FocusStack.SetCapacity(MAX_STACK_COUNT);
        *world = cam_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraDeleteWorld(void* context, void* world)
    {
        delete (CameraWorld*)world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCameraCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        CameraWorld* w = (CameraWorld*)world;
        if (!w->m_Cameras.Full())
        {
            dmGameSystem::CameraResource* cam_resource = (CameraResource*)resource;
            CameraComponent camera;
            camera.m_Instance = instance;
            camera.m_World = w;
            camera.m_AspectRatio = cam_resource->m_DDF->m_AspectRatio;
            camera.m_Fov = cam_resource->m_DDF->m_Fov;
            camera.m_NearZ = cam_resource->m_DDF->m_NearZ;
            camera.m_FarZ = cam_resource->m_DDF->m_FarZ;
            camera.m_AutoAspectRatio = cam_resource->m_DDF->m_AutoAspectRatio != 0;
            w->m_Cameras.Push(camera);
            *user_data = (uintptr_t)&w->m_Cameras[w->m_Cameras.Size() - 1];
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Camera buffer is full (%d), component disregarded.", MAX_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompCameraDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        CameraWorld* w = (CameraWorld*)world;
        CameraComponent* camera = (CameraComponent*)*user_data;
        bool found = false;
        for (uint8_t i = 0; i < w->m_FocusStack.Size(); ++i)
        {
            if (w->m_FocusStack[i] == camera)
            {
                found = true;
            }
            if (found && i < w->m_FocusStack.Size() - 1)
            {
                w->m_FocusStack[i] = w->m_FocusStack[i + 1];
            }
        }
        if (found)
        {
            w->m_FocusStack.Pop();
        }
        for (uint8_t i = 0; i < w->m_Cameras.Size(); ++i)
        {
            if (w->m_Cameras[i].m_Instance == instance)
            {
                w->m_Cameras.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        dmLogError("Destroyed camera could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::UpdateResult CompCameraUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context)
    {
        CameraWorld* w = (CameraWorld*)world;
        CameraComponent* camera = 0x0;
        if (w->m_FocusStack.Size() > 0)
        {
            camera = w->m_FocusStack[w->m_FocusStack.Size() - 1];
        }
        if (camera != 0x0)
        {
            dmRender::RenderContext* render_context = (dmRender::RenderContext*)context;

            float aspect_ratio = camera->m_AspectRatio;
            if (camera->m_AutoAspectRatio)
            {
                float width = (float)dmGraphics::GetWindowWidth(dmRender::GetGraphicsContext(render_context));
                float height = (float)dmGraphics::GetWindowHeight(dmRender::GetGraphicsContext(render_context));
                aspect_ratio = width / height;
            }
            Vectormath::Aos::Matrix4 projection = Matrix4::perspective(camera->m_Fov, aspect_ratio, camera->m_NearZ, camera->m_FarZ);

            Vectormath::Aos::Point3 pos = dmGameObject::GetWorldPosition(camera->m_Instance);
            Vectormath::Aos::Quat rot = dmGameObject::GetWorldRotation(camera->m_Instance);
            Point3 look_at = pos + Vectormath::Aos::rotate(rot, Vectormath::Aos::Vector3(0.0f, 0.0f, -1.0f));
            Vector3 up = Vectormath::Aos::rotate(rot, Vectormath::Aos::Vector3(0.0f, 1.0f, 0.0f));
            Vectormath::Aos::Matrix4 view = Matrix4::lookAt(pos, look_at, up);

            // Send the matrices to the render script
            char buf[sizeof(dmGameObject::InstanceMessageData) + sizeof(dmGameSystemDDF::SetViewProjection) + 9];
            dmGameSystemDDF::SetViewProjection* set_view_projection = (dmGameSystemDDF::SetViewProjection*) (buf + sizeof(dmGameObject::InstanceMessageData));


            dmMessage::HSocket socket_id = dmMessage::GetSocket("render");
            dmhash_t message_id = dmHashString64(dmGameSystemDDF::SetViewProjection::m_DDFDescriptor->m_Name);

            set_view_projection->m_Id = dmHashString64("game");
            set_view_projection->m_View = view;
            set_view_projection->m_Projection = projection;

            dmGameObject::InstanceMessageData* msg = (dmGameObject::InstanceMessageData*) buf;
            msg->m_BufferSize = sizeof(dmGameSystemDDF::SetViewProjection) + 9;
            msg->m_DDFDescriptor = dmGameSystemDDF::SetViewProjection::m_DDFDescriptor;
            msg->m_MessageId = message_id;

            dmMessage::Post(socket_id, message_id, buf, sizeof(buf));

            // Set matrices immediately
            // TODO: Remove this once render scripts are implemented everywhere
            dmRender::SetProjectionMatrix(render_context, projection);
            dmRender::SetViewMatrix(render_context, view);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCameraOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        CameraComponent* camera = (CameraComponent*)*user_data;
        if (message_data->m_DDFDescriptor == dmGamesysDDF::SetCamera::m_DDFDescriptor)
        {
            dmGamesysDDF::SetCamera* ddf = (dmGamesysDDF::SetCamera*)message_data->m_Buffer;
            camera->m_AspectRatio = ddf->m_AspectRatio;
            camera->m_Fov = ddf->m_Fov;
            camera->m_NearZ = ddf->m_NearZ;
            camera->m_FarZ = ddf->m_FarZ;
        }
        else if (message_data->m_MessageId == dmHashString64("acquire_camera_focus"))
        {
            bool found = false;
            for (uint32_t i = 0; i < camera->m_World->m_FocusStack.Size(); ++i)
            {
                if (camera->m_World->m_FocusStack[i] == camera)
                {
                    found = true;
                }
                if (found && i < camera->m_World->m_FocusStack.Size() - 1)
                {
                    camera->m_World->m_FocusStack[i] = camera->m_World->m_FocusStack[i + 1];
                }
            }
            if (found)
            {
                camera->m_World->m_FocusStack.Pop();
            }
            if (!camera->m_World->m_FocusStack.Full())
            {
                camera->m_World->m_FocusStack.Push(camera);
            }
            else
            {
                dmLogWarning("Could not acquire camera focus since the buffer is full (%d).", camera->m_World->m_FocusStack.Size());
            }
        }
        else if (message_data->m_MessageId == dmHashString64("release_camera_focus"))
        {
            bool found = false;
            for (uint32_t i = 0; i < camera->m_World->m_FocusStack.Size(); ++i)
            {
                if (camera->m_World->m_FocusStack[i] == camera)
                {
                    found = true;
                }
                if (found && i < camera->m_World->m_FocusStack.Size() - 1)
                {
                    camera->m_World->m_FocusStack[i] = camera->m_World->m_FocusStack[i + 1];
                }
            }
            if (found)
            {
                camera->m_World->m_FocusStack.Pop();
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCameraOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        CameraResource* cam_resource = (CameraResource*)resource;
        CameraComponent* camera = (CameraComponent*)*user_data;
        camera->m_AspectRatio = cam_resource->m_DDF->m_AspectRatio;
        camera->m_Fov = cam_resource->m_DDF->m_Fov;
        camera->m_NearZ = cam_resource->m_DDF->m_NearZ;
        camera->m_FarZ = cam_resource->m_DDF->m_FarZ;
        camera->m_AutoAspectRatio = cam_resource->m_DDF->m_AutoAspectRatio != 0;
    }
}
