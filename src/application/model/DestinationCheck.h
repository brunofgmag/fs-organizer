#ifndef FS_ORGANIZER_APPLICATION_MODEL_DESTINATION_CHECK_H
#define FS_ORGANIZER_APPLICATION_MODEL_DESTINATION_CHECK_H

enum class DestinationCheck : int
{
    Accepted = 0,
    AcceptedButUnfamiliar = 1,
    RejectedMissing = 2,
    RejectedNotWritable = 3,
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_DESTINATION_CHECK_H
