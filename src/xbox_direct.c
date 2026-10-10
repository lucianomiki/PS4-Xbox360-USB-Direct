/* Xbox 360 USB-native input source for Control4Free.
 * This is an experimental PS4 payload component, NOT a GoldHEN game plugin.
 * GPL-3.0-or-later: https://github.com/MoHadiShibli/Control4Free
 *
 * Only Sony's already-tested virtual device driver publishes input to games.
 * No scePadOpen/Read/ReadState hooks. No browser, phone or PC is needed at runtime.
 * USB access is dynamically resolved: ps4-payload-sdk does not ship libSceUsbd.
 */
#include "c4f_xbox_direct.h"
#include "c4f_log.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0x100
#endif

/* libusb's standard USB device descriptor is 18 bytes, packed. */
typedef struct __attribute__((packed)) {
    uint8_t length, descriptorType;
    uint16_t bcdUSB;
    uint8_t deviceClass, deviceSubClass, deviceProtocol, maxPacketSize0;
    uint16_t vendor, product, bcdDevice;
    uint8_t manufacturer, productString, serialNumber, configurations;
} UsbDeviceDesc;
_Static_assert(sizeof(UsbDeviceDesc) == 18, "USB descriptor layout must be 18 bytes");

typedef void UsbDev;
typedef void UsbHandle;

typedef struct {
    int32_t (*init)(void);
    int32_t (*exit)(void);
    int32_t (*list)(UsbDev ***);
    int32_t (*freeList)(UsbDev **);
    int32_t (*descriptor)(UsbDev *, UsbDeviceDesc *);
    int32_t (*open)(UsbDev *, UsbHandle **);
    void (*close)(UsbHandle *);
    int32_t (*claim)(UsbHandle *, int32_t);
    int32_t (*release)(UsbHandle *, int32_t);
    int32_t (*detach)(UsbHandle *, int32_t);
    int32_t (*checkConnected)(UsbHandle *); /* optional, queried only after inactivity */
    int32_t (*interrupt)(UsbHandle *, unsigned char, unsigned char *, int32_t, int32_t *, uint32_t);
} UsbFns;

static UsbFns f;
static void *lib;
static UsbHandle *handle;
static pthread_t worker;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int started;
static _Atomic int running;
static int usbConnected; /* guarded by mutex */
static uint8_t lastReport[20];
static uint64_t lastInputMs;
static uint32_t lastSequence;

static uint64_t nowMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* The USB worker exclusively owns the active USB handle while running.
 * This is deliberately separate from Sony's VDA thread/game processes. */
static UsbHandle *openMatchingUsb(void)
{
    UsbDev **devices = NULL;
    int32_t n = f.list(&devices);
    UsbHandle *result = NULL;
    if (n <= 0 || !devices) {
        if (devices) f.freeList(devices);
        return NULL;
    }
    for (int32_t i = 0; i < n; i++) {
        UsbDeviceDesc d = {0};
        if (f.descriptor(devices[i], &d) != 0 ||
            d.vendor != 0x045e || d.product != 0x028e) continue;
        UsbHandle *candidate = NULL;
        if (f.open(devices[i], &candidate) != 0 || !candidate) continue;
        (void)f.detach(candidate, 0);
        if (f.claim(candidate, 0) == 0) {
            result = candidate;
            break;
        }
        f.close(candidate);
    }
    f.freeList(devices);
    return result;
}

/* Going offline must neutralize an old pressed button immediately. The
 * PS4 virtual pad itself is NEVER deleted/recreated on a cable unplug. */
static void setUsbConnected(int online)
{
    pthread_mutex_lock(&mutex);
    usbConnected = online;
    memset(lastReport, 0, sizeof(lastReport));
    lastInputMs = 0;
    lastSequence++;
    pthread_mutex_unlock(&mutex);
}

static void *readWorker(void *unused)
{
    (void)unused;
    uint64_t lastValid = nowMs();
    uint64_t lastCheck = 0;
    int failedChecks = 0;
    int otherErrors = 0;

    while (atomic_load(&running)) {
        if (!handle) {
            /* An unplugged Xbox should not destroy its PS4 user assignment.
             * Rescan at low frequency so the disconnected state uses no CPU. */
            UsbHandle *fresh = openMatchingUsb();
            if (fresh) {
                handle = fresh;
                setUsbConnected(1);
                lastValid = nowMs();
                lastCheck = 0;
                failedChecks = otherErrors = 0;
                c4fLog("USB direct v0.2: Xbox reconnected; original virtual pad retained\n");
            } else {
                usleep(750000);
            }
            continue;
        }

        unsigned char bytes[20] = {0};
        int32_t length = 0;
        /* Ordinary USB timeouts while idle are NOT disconnections.
         * All USB waits happen exclusively in this background thread. */
        int32_t ret = f.interrupt(handle, 0x81, bytes, 20, &length, 12);
        uint64_t now = nowMs();
        if (ret == 0 && length == 20 && bytes[0] == 0 && bytes[1] == 0x14) {
            pthread_mutex_lock(&mutex);
            memcpy(lastReport, bytes, 20);
            lastInputMs = now;
            lastSequence++;
            pthread_mutex_unlock(&mutex);
            lastValid = now;
            failedChecks = otherErrors = 0;
        } else {
            /* Firmware 13.02 returns 0x80240007 for USB timeout.
             * Do not act on transient timeouts; fall back to neutral after
             * 350 ms through c4fXboxDirectSample(), as in working v0.1. */
            if (ret != 0 && (uint32_t)ret != 0x80240007u && otherErrors < 1000)
                otherErrors++;
            if (now >= lastValid && now - lastValid >= 650 &&
                now >= lastCheck && now - lastCheck >= 400) {
                lastCheck = now;
                if (f.checkConnected) {
                    if (f.checkConnected(handle) != 0) failedChecks++;
                    else failedChecks = 0;
                } else if (otherErrors >= 16) {
                    /* Degraded detection when CheckConnected isn't exported. */
                    failedChecks = 2;
                }
            }
        }

        if (failedChecks >= 2) {
            setUsbConnected(0);
            c4fLog("USB direct v0.2: Xbox disconnected; inputs neutral, scanning for USB reconnect\n");
            f.release(handle, 0);
            f.close(handle);
            handle = NULL;
            lastValid = nowMs();
            lastCheck = 0;
            failedChecks = otherErrors = 0;
            usleep(500000);
            continue;
        }
        usleep(4000); /* Same polling cap as functional v0.1. */
    }
    return NULL;
}

#define LOAD_FUNCTION(member, symbol) do { \
    *(void **)(&f.member) = dlsym(lib, symbol); \
    if (!f.member) { c4fLog("USB direct: missing %s\n", symbol); goto fail; } \
} while (0)

int c4fXboxDirectStart(void)
{
    if (started) return 1;
    static const char *names[] = {
        "/system/common/lib/libSceUsbd.sprx",
        "libSceUsbd.sprx",
        "libSceUsbd"
    };
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]) && !lib; i++)
        lib = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
    if (!lib) { c4fLog("USB direct: libSceUsbd unavailable\n"); return 0; }

    LOAD_FUNCTION(init, "sceUsbdInit");
    LOAD_FUNCTION(exit, "sceUsbdExit");
    LOAD_FUNCTION(list, "sceUsbdGetDeviceList");
    LOAD_FUNCTION(freeList, "sceUsbdFreeDeviceList");
    LOAD_FUNCTION(descriptor, "sceUsbdGetDeviceDescriptor");
    LOAD_FUNCTION(open, "sceUsbdOpen");
    LOAD_FUNCTION(close, "sceUsbdClose");
    LOAD_FUNCTION(claim, "sceUsbdClaimInterface");
    LOAD_FUNCTION(release, "sceUsbdReleaseInterface");
    LOAD_FUNCTION(detach, "sceUsbdDetachKernelDriver");
    LOAD_FUNCTION(interrupt, "sceUsbdInterruptTransfer");
    /* Optional on some builds; don't break the working USB initialization
     * if this export is absent. Reconnection then relies on fatal I/O errors. */
    *(void **)(&f.checkConnected) = dlsym(lib, "sceUsbdCheckConnected");
    {
        int32_t ret = f.init();
        if (ret != 0) { c4fLog("USB direct: sceUsbdInit=0x%08x\n", (uint32_t)ret); goto fail; }
    }
    handle = openMatchingUsb();
    if (!handle) {
        c4fLog("USB direct v0.2: Xbox 360 not available at startup; connect before launching\n");
        goto fail_initialized;
    }

    pthread_mutex_lock(&mutex);
    memset(lastReport, 0, sizeof(lastReport));
    lastSequence = 0; lastInputMs = 0; usbConnected = 1;
    pthread_mutex_unlock(&mutex);
    atomic_store(&running, 1);
    if (pthread_create(&worker, NULL, readWorker, NULL) != 0) {
        c4fLog("USB direct: pthread_create failed\n");
        atomic_store(&running, 0);
        f.release(handle, 0); f.close(handle); handle = NULL;
        goto fail_initialized;
    }
    started = 1;
    c4fLog("USB direct: Xbox 360 claimed; worker started\n");
    return 1;

fail_initialized:
    f.exit();
fail:
    if (lib) dlclose(lib);
    lib = NULL;
    memset(&f, 0, sizeof(f));
    return 0;
}

void c4fXboxDirectStop(void)
{
    if (!started) return;
    atomic_store(&running, 0);
    pthread_join(worker, NULL);
    if (handle) { f.release(handle, 0); f.close(handle); handle = NULL; }
    setUsbConnected(0);
    f.exit();
    if (lib) dlclose(lib);
    lib = NULL;
    started = 0;
    memset(&f, 0, sizeof(f));
}

static uint8_t axis(uint8_t low, uint8_t high)
{
    /* Avoid implementation-defined signed shifts: signed 16-bit -> DS4 8-bit. */
    return (uint8_t)((((int16_t)((uint16_t)low | ((uint16_t)high << 8))) + 32768) >> 8);
}

int c4fXboxDirectSample(ScePadData *out, uint32_t *sequence)
{
    uint8_t b[20];
    uint64_t then;
    uint32_t seq;
    memset(out, 0, sizeof(*out));
    out->lx = out->ly = out->rx = out->ry = 128;
    out->quat[3] = 1.0f;
    out->connected = 1;
    out->count = 1;
    if (!started) { if (sequence) *sequence = 0; return 0; }
    pthread_mutex_lock(&mutex);
    memcpy(b, lastReport, sizeof(b));
    then = lastInputMs;
    seq = lastSequence;
    int online = usbConnected;
    pthread_mutex_unlock(&mutex);
    if (sequence) *sequence = seq;
    if (!online || !then || nowMs() - then > 350) return 0; /* release stale buttons */

    const uint8_t lo=b[2], hi=b[3];
    if (lo & 0x01) out->buttons |= 0x0010; /* Up */
    if (lo & 0x02) out->buttons |= 0x0040; /* Down */
    if (lo & 0x04) out->buttons |= 0x0080; /* Left */
    if (lo & 0x08) out->buttons |= 0x0020; /* Right */
    if (lo & 0x10) out->buttons |= 0x0008; /* Start -> Options */
    if (lo & 0x20) out->buttons |= 0x0001; /* Back -> Share */
    if (lo & 0x40) out->buttons |= 0x0002; /* L3 */
    if (lo & 0x80) out->buttons |= 0x0004; /* R3 */
    if (hi & 0x01) out->buttons |= 0x0400; /* LB */
    if (hi & 0x02) out->buttons |= 0x0800; /* RB */
    if (hi & 0x04) out->buttons |= 0x10000;/* Xbox -> PS */
    if (hi & 0x10) out->buttons |= 0x4000; /* A -> Cross */
    if (hi & 0x20) out->buttons |= 0x2000; /* B -> Circle */
    if (hi & 0x40) out->buttons |= 0x8000; /* X -> Square */
    if (hi & 0x80) out->buttons |= 0x1000; /* Y -> Triangle */
    out->l2 = b[4]; out->r2 = b[5];
    if (b[4] >= 30) out->buttons |= 0x0100;
    if (b[5] >= 30) out->buttons |= 0x0200;
    out->lx = axis(b[6], b[7]);
    out->ly = (uint8_t)(255 - axis(b[8], b[9]));
    out->rx = axis(b[10], b[11]);
    out->ry = (uint8_t)(255 - axis(b[12], b[13]));
    return 1;
}

/* Is the USB cable/controller currently considered connected? This does not
 * inspect USB from the game's thread: only a mutex-protected cached flag. */
int c4fXboxDirectConnected(void)
{
    int online;
    pthread_mutex_lock(&mutex);
    online = usbConnected;
    pthread_mutex_unlock(&mutex);
    return online;
}
