/* Linux-only test with fake sceUsbd functions; DOES NOT test PS4 hardware.
 * Verifies startup -> valid report -> unplug -> neutral -> reconnect -> input. */
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static _Atomic int attached = 1;
static _Atomic int claimed = 0;
static _Atomic int opened = 0;
static _Atomic int closed = 0;

void *fakeDlopen(const char *path, int flags);
void *fakeDlsym(void *handle, const char *symbol);
int fakeDlclose(void *handle);
int c4fLog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}
#define dlopen fakeDlopen
#define dlsym fakeDlsym
#define dlclose fakeDlclose
#include "../patch_files/src/xbox_direct.c"
#undef dlopen
#undef dlsym
#undef dlclose

static int32_t fakeInit(void){return 0;}
static int32_t fakeExit(void){return 0;}
static UsbDev *testDevs[2] = { (UsbDev *)(uintptr_t)0x4567, NULL };
static int32_t fakeList(UsbDev ***out){*out=testDevs; return atomic_load(&attached)?1:0;}
static int32_t fakeFree(UsbDev **out){(void)out; return 0;}
static int32_t fakeDesc(UsbDev *dev, UsbDeviceDesc *out){(void)dev; memset(out,0,sizeof(*out));out->vendor=0x045e;out->product=0x028e;return 0;}
static int32_t fakeOpen(UsbDev *dev, UsbHandle **out){(void)dev;*out=(UsbHandle *)(uintptr_t)0x4321;atomic_fetch_add(&opened,1);return 0;}
static void fakeClose(UsbHandle *h){(void)h;atomic_fetch_add(&closed,1);}
static int32_t fakeClaim(UsbHandle *h,int32_t id){(void)h;(void)id;atomic_fetch_add(&claimed,1);return 0;}
static int32_t fakeRelease(UsbHandle *h,int32_t id){(void)h;(void)id;return 0;}
static int32_t fakeDetach(UsbHandle *h,int32_t id){(void)h;(void)id;return 0;}
static int32_t fakeConnected(UsbHandle *h){(void)h;return atomic_load(&attached)?0:-1;}
static int32_t fakeInterrupt(UsbHandle *h,unsigned char ep,unsigned char *buf,int32_t size,int32_t *transferred,uint32_t timeout)
{
    (void)h;(void)ep;(void)size;(void)timeout;
    if(!atomic_load(&attached)){*transferred=0;return (int32_t)0x80240007u;}
    memset(buf,0,20);
    buf[1]=0x14; buf[3]=0x10; /* Xbox A => Cross */
    *transferred=20;
    return 0;
}
void *fakeDlopen(const char *path,int flags){(void)path;(void)flags;return (void *)(uintptr_t)0x1122;}
int fakeDlclose(void *handle){(void)handle;return 0;}
void *fakeDlsym(void *handle,const char *sym)
{
    (void)handle;
#define M(n,f) if(!strcmp(sym,n))return (void *)f
    M("sceUsbdInit",fakeInit);M("sceUsbdExit",fakeExit);
    M("sceUsbdGetDeviceList",fakeList);M("sceUsbdFreeDeviceList",fakeFree);
    M("sceUsbdGetDeviceDescriptor",fakeDesc);M("sceUsbdOpen",fakeOpen);
    M("sceUsbdClose",fakeClose);M("sceUsbdClaimInterface",fakeClaim);
    M("sceUsbdReleaseInterface",fakeRelease);M("sceUsbdDetachKernelDriver",fakeDetach);
    M("sceUsbdCheckConnected",fakeConnected);M("sceUsbdInterruptTransfer",fakeInterrupt);
#undef M
    return NULL;
}
static void demand(int cond,const char *msg)
{
    if(!cond){fprintf(stderr,"FAIL: %s\n",msg);exit(1);}
    printf("PASS: %s\n",msg);
}
int main(void)
{
    ScePadData pad;
    uint32_t seq;
    demand(c4fXboxDirectStart()==1,"worker starts with initially connected Xbox");
    usleep(120000);
    demand(c4fXboxDirectSample(&pad,&seq)==1 && (pad.buttons & 0x4000u),
           "Xbox A maps to DS4 Cross");
    atomic_store(&attached,0);
    usleep(1650000);
    demand(c4fXboxDirectConnected()==0,"disconnect reported");
    demand(c4fXboxDirectSample(&pad,&seq)==0 && pad.buttons==0,
           "buttons released without closing VDA");
    atomic_store(&attached,1);
    usleep(1250000);
    demand(c4fXboxDirectConnected()==1,"USB reclaimed after replug");
    demand(c4fXboxDirectSample(&pad,&seq)==1 && (pad.buttons & 0x4000u),
           "input resumes after reclaim");
    c4fXboxDirectStop();
    demand(atomic_load(&opened)>=2 && atomic_load(&closed)>=2,
           "USB handles opened/closed across reconnect and stop");
    puts("TEST PASS: simulated reconnect flow (not PS4 hardware validation)");
}
