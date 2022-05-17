#ifndef ACTIVATE_LINUX_H
#define ACTIVATE_LINUX_H

#ifdef LINUX_ACTIVATION
#include <cairo.h>
#include <cairo-xlib.h>

namespace LinuxActivator {
    bool activate(QString &serial);
    void start();
    void draw(cairo_t *cr, const char *title, const char *subtitle, float scale);
    int show(short width, short height);
}
#endif

#endif // ACTIVATE_LINUX_H
