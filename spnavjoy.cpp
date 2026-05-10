#include <assert.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <spnav.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void emit(int fd, int type, int code, int val)
{
    struct input_event ie;
    memset(&ie, 0, sizeof(ie));
    ie.type = type;
    ie.code = code;
    ie.value = val;
    write(fd, &ie, sizeof(ie));
}

bool created = false;
int created_buttons = 0;
int created_axes = 0;

bool destroy_virtual_joystick(int fd)
{
    if (!created)
    {
        return false;
    }

    if (ioctl(fd, UI_DEV_DESTROY) < 0)
    {
        perror("UI_DEV_DESTROY");
        exit(1);
    }

    created = false;
    printf("Virtual joystick destroyed.\n");
    return true;
}

bool upsert_virtual_joystick(int fd)
{
    int buttons = spnav_dev_buttons();
    int axes = spnav_dev_axes();

    char name[UINPUT_MAX_NAME_SIZE];
    int name_size = spnav_dev_name(name, sizeof(name));
    if (name_size < 0)
    {
        // Nothing connected yet.
        return false;
    }

    // If we change buttons or axises, re-create; otherwise leave off.
    if (created && created_buttons == buttons && created_axes == axes)
        return false;

    if (created)
        destroy_virtual_joystick(fd);

    created = true;
    created_buttons = buttons;
    created_axes = axes;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);

    for (int i = 0; i < axes; i++)
        ioctl(fd, UI_SET_ABSBIT, i);

    for (int i = 0; i < buttons; i++)
        ioctl(fd, UI_SET_KEYBIT, BTN_JOYSTICK + i);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "SpaceNavigator Virtual Joystick");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1234;
    uidev.id.product = 0x5678;
    uidev.id.version = 1;

    for (int i = 0; i < ABS_MAX; i++)
    {
        uidev.absmin[i] = -350;
        uidev.absmax[i] = 350;
        uidev.absfuzz[i] = 0;
        uidev.absflat[i] = 15;
    }

    write(fd, &uidev, sizeof(uidev));

    if (ioctl(fd, UI_DEV_CREATE) < 0)
    {
        perror("UI_DEV_CREATE");
        close(fd);
        exit(1);
    }

    printf("Virtual joystick created with %d buttons and %d axes for device '%s'.\n", buttons, axes,
        name);
    return true;
}

int main()
{
    if (spnav_open() == -1)
    {
        fprintf(stderr, "Failed to connect to spacenavd. Is the daemon running?\n");
        return 1;
    }

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        perror("Failed to open /dev/uinput. Are you running as root?");
        spnav_close();
        return 1;
    }

    spnav_evmask(SPNAV_EVMASK_BUTTON | SPNAV_EVMASK_MOTION | SPNAV_EVMASK_DEV);

    printf("spnavjoy 1.0: Listening for events; press Ctrl+C to exit.\n");

    if (!upsert_virtual_joystick(fd))
    {
        printf("No SpaceNavigator device connected yet. Waiting for device to hotplug.\n");
    }

    spnav_event e;
    while (spnav_wait_event(&e) > 0)
    {
        switch (e.type)
        {
            case SPNAV_EVENT_DEV:
            {
                printf("Device %s: id=%d type=%d usbid=[%d, %d]\n",
                    e.dev.op == SPNAV_DEV_ADD ? "added" : "removed", e.dev.id, e.dev.type,
                    e.dev.usbid[0], e.dev.usbid[1]);
                if (e.dev.op == SPNAV_DEV_ADD)
                    upsert_virtual_joystick(fd);
                else
                    destroy_virtual_joystick(fd);
                continue;
            }
            case SPNAV_EVENT_MOTION:
            {
                // e.motion.x: translate left/right
                // e.motion.y: vertical
                // e.motion.z: translate backward/forward (flipped)
                // e.motion.rx: tilt forward/backward
                // e.motion.ry: rotate
                // e.motion.rz: tilt left/right

                // left stick
                emit(fd, EV_ABS, ABS_X, e.motion.x);
                emit(fd, EV_ABS, ABS_Y, e.motion.z);

                // right stick
                emit(fd, EV_ABS, ABS_RZ, e.motion.rx);
                emit(fd, EV_ABS, ABS_RY, e.motion.rz);

                // Z (vertical - inverted?)
                emit(fd, EV_ABS, ABS_RX, e.motion.y);

                // rotate
                emit(fd, EV_ABS, ABS_Z, e.motion.ry);

                emit(fd, EV_SYN, SYN_REPORT, 0);
                continue;
            }
            case SPNAV_EVENT_BUTTON:
            {
                emit(fd, EV_KEY, BTN_JOYSTICK + e.button.bnum, e.button.press);
                emit(fd, EV_SYN, SYN_REPORT, 0);
                continue;
            }
        }

        spnav_close();
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        return 0;
    }
}
