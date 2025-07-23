#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD FilterEvtAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL FilterEvtIoctl;

static UNICODE_STRING g_vhid_sym = RTL_CONSTANT_STRING(
    L"\\??\\ScreenSwitchVirtualHid");

NTSTATUS DriverEntry(PDRIVER_OBJECT d, PUNICODE_STRING r) {
    WDF_DRIVER_CONFIG c; WDF_DRIVER_CONFIG_INIT(&c, FilterEvtAdd);
    return WdfDriverCreate(d, r, WDF_NO_OBJECT_ATTRIBUTES, &c, WDF_NO_HANDLE);
}

NTSTATUS FilterEvtAdd(WDFDRIVER, WDFDEVICE_INIT* init) {
    WdfFdoInitSetFilter(init);
    WDFDEVICE dev; NTSTATUS s = WdfDeviceCreate(&init, {}, &dev); if (!NT_SUCCESS(s))return s;
    WDF_IO_QUEUE_CONFIG q; WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q, WdfIoQueueDispatchParallel);
    q.EvtIoInternalDeviceControl = FilterEvtIoctl;
    return WdfIoQueueCreate(dev, &q, {}, WDF_NO_HANDLE);
}

VOID FilterEvtIoctl(WDFQUEUE q, WDFREQUEST r, size_t o, size_t i, ULONG code) {
    UNREFERENCED_PARAMETER(i);
    if (code == IOCTL_HID_READ_REPORT) {
        PUCHAR buf = nullptr; size_t len = 0;
        NTSTATUS s = WdfRequestRetrieveOutputBuffer(r, 0, (PVOID*)&buf, &len);
        if (NT_SUCCESS(s) && len) {
            /* ��: ��ư1(0x01)������ ���� 0x10 */
            if (buf[0] == 0x01) buf[0] = 0x10;
            /* ���� HID�� �״�� ���� */
            WDF_OBJECT_ATTRIBUTES attrs;
            WDF_OBJECT_ATTRIBUTES_INIT(&attrs);          // �Ǵ� WDF_NO_OBJECT_ATTRIBUTES
            WDFIOTARGET tgt;
            WdfIoTargetCreate(WdfIoQueueGetDevice(q), &attrs, &tgt);
            WDF_IO_TARGET_OPEN_PARAMS p;
            WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&p, &g_vhid_sym, GENERIC_WRITE);
            NTSTATUS s = WdfIoTargetOpen(tgt, &p);
            if (NT_SUCCESS(s)) {
                WDF_MEMORY_DESCRIPTOR md; WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&md, buf, (ULONG)len);
                WdfIoTargetSendWriteSynchronously(tgt, nullptr, &md, nullptr, nullptr, nullptr);
                WdfIoTargetClose(tgt);
            }
            WdfRequestCompleteWithInformation(r, STATUS_SUCCESS, len); return;
        }
        WdfRequestComplete(r, s); return;
    }
    /* ��Ÿ IOCTL�� �⺻ �������� */
    WdfRequestForwardToIoQueue(r, WdfDeviceGetDefaultQueue(WdfIoQueueGetDevice(q)));
}
