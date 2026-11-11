#include <phx/device/partition.h>

void PHX_Partition_CreateRandomSignature(PHX_Context* context, PHX_Partition_Signature* signature, PHX_Partition_Signature_Type type)
{
    switch (type)
    {
        case PHX_PARTITION_SIGNATURE_32:
            signature->first = PHX_Context_GetRandomU32(context);
            signature->second = 0;
            break;

        case PHX_PARTITION_SIGNATURE_GUID:
            signature->first = PHX_Context_GetRandomU64(context);
            signature->second = PHX_Context_GetRandomU64(context);
            break;

        case PHX_PARTITION_SIGNATURE_NONE: default: // TODO
            signature->first = 0;
            signature->second = 0;
            break;
    }

    signature->type = type;
}
