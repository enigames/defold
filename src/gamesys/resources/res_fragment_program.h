#ifndef DM_GAMESYS_RES_FRAGMENT_PROGRAM_H
#define DM_GAMESYS_RES_FRAGMENT_PROGRAM_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResFragmentProgramCreate(dmResource::HFactory factory,
                                                   void* context,
                                                   const void* buffer, uint32_t buffer_size,
                                                   dmResource::SResourceDescriptor* resource,
                                                   const char* filename);

    dmResource::CreateResult ResFragmentProgramDestroy(dmResource::HFactory factory,
                                                    void* context,
                                                    dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResFragmentProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename);
}

#endif // DM_GAMESYS_RES_FRAGMENT_PROGRAM_H
