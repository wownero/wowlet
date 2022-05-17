#ifdef LINUX_ACTIVATION

#include <QObject>
#include <QApplication>
#include <QPixmap>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QCoreApplication>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QGroupBox>
#include <QFileDialog>

#include <thread>
#include <cstdio>
#include <cstdlib>

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include "activate_linux.h"
#include "utils/config.h"

namespace LinuxActivator
{
    bool activate(QString &serial) {
        auto isActivated = config()->get(Config::LinuxActivated).toString();
        if(!isActivated.isEmpty())
            return true;

        QStringList serials{
            "bd898635-6fbe-4cc8-bf50-adee9f76f091",
            "6c153b1c-da23-41cf-a606-8888436d4efb",
            "b8962f75-35fd-4237-9732-dd14eb9c71c7",
            "6f5f8449-efab-4666-86a5-a25da0c6c5bd",
            "5ef5e95b-a1ef-489a-9583-6cdb0b369ff4",
            "a055609a-7873-42fe-9644-f6261d32fa57",
            "772f7941-0d1c-4158-9c09-701198e71dff",
            "d7f9c0d0-6361-4145-831f-490d396bb30f",
            "68aa6270-7765-437d-8aab-e09c56b9f897",
            "7f7e2aef-96a4-4420-b0d8-86135019fcc2"
        };

        serial = serial.trimmed();
        if(serials.contains(serial)) {
            config()->set(Config::LinuxActivated, serial);
            return true;
        }
        return false;
    }

    // fire & forget
    void start() {
        QRect rec = QApplication::desktop()->screenGeometry();
        std::thread([rec](){
            const short height = rec.height();
            const short width = rec.width();
            LinuxActivator::show(width, height);
        }).detach();
    }

    // draw text
    void draw(cairo_t *cr, const char *title, const char *subtitle, float scale) {
        //set color
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);

        // set font size, and scale up or down
        cairo_set_font_size(cr, 24 * scale);
        cairo_move_to(cr, 20, 30 * scale);
        cairo_show_text(cr, title);

        cairo_set_font_size(cr, 16 * scale);
        cairo_move_to(cr, 20, 55 * scale);
        cairo_show_text(cr, subtitle);
    }

    int show(const short width, const short height) {
        Display *d = XOpenDisplay(NULL);
        Window root = DefaultRootWindow(d);
        int default_screen = XDefaultScreen(d);

        char const *title = "Activate Wownero";
        char const *subtitle = "Go to Settings to hacktivate Wownero.";

        // default scale
        float scale = 1.0f;
        if(width >= 2560) {
            scale = 1.2f;
        } else if(width >= 3440) {
            scale = 1.5f;
        }

        int overlay_width = 380;
        int overlay_height = 120;
        overlay_height *= scale;
        overlay_width *= scale;

        XSetWindowAttributes attrs;
        attrs.override_redirect = 1;

        XVisualInfo vinfo;
        int colorDepth = 32;

        if (!XMatchVisualInfo(d, default_screen, colorDepth, TrueColor, &vinfo)) {
            printf("Activate linux: no visual found supporting 32 bit color, terminating\n");
            exit(EXIT_FAILURE);
        }

        // sets 32 bit color depth
        attrs.colormap = XCreateColormap(d, root, vinfo.visual, AllocNone);
        attrs.background_pixel = 0;
        attrs.border_pixel = 0;

        Window overlay;
        cairo_surface_t *surface;
        cairo_t *cairo_ctx;

        overlay = XCreateWindow(
            d,                                                 // display
            root,                                              // parent
            width - overlay_width,                             // x position
            height - overlay_height,                           // y position
            overlay_width,                                     // width
            overlay_height,                                    // height
            0,                                                 // border width
            vinfo.depth,                                       // depth
            InputOutput,                                       // class
            vinfo.visual,                                      // visual
            CWOverrideRedirect |                               // value mask
            CWColormap |
            CWBackPixel |
            CWBorderPixel,
            &attrs                                             // attributes
        );
        XMapWindow(d, overlay);

        // allows the mouse to click through the overlay
        XRectangle rect;
        XserverRegion region = XFixesCreateRegion(d, &rect, 1);
        XFixesSetWindowShapeRegion(d, overlay, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(d, region);

        // sets a WM_CLASS to allow the user to blacklist some effect from compositor
        XClassHint *xch = XAllocClassHint();
        xch->res_name = "activate-linux";
        xch->res_class = "activate-linux";
        XSetClassHint(d, overlay, xch);

        // cairo context
        surface = cairo_xlib_surface_create(d, overlay, vinfo.visual, overlay_width, overlay_height);
        cairo_ctx = cairo_create(surface);
        draw(cairo_ctx, title, subtitle, scale);

        // wait for X events forever
        XEvent event;
        while(1) {
            XNextEvent(d, &event);
        }

        // free used resources
        XUnmapWindow(d, overlay);
        cairo_destroy(cairo_ctx);
        cairo_surface_destroy(surface);

        XCloseDisplay(d);
        return 0;
    }
}

#endif