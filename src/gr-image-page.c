/* gr-image-page.c:
 *
 * Copyright (C) 2017 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gr-image-page.h"
#include "gr-images.h"
#include "gr-utils.h"
#include "gr-window.h"


struct _GrImagePage
{
        GtkBox parent_instance;

        GtkWidget *image;
        GtkWidget *event_box;
        GtkWidget *next_revealer;
        GtkWidget *prev_revealer;
        GtkWidget *close_revealer;

        GArray *images;
        int index;

        guint hide_timeout;
};


G_DEFINE_TYPE (GrImagePage, gr_image_page, GTK_TYPE_BOX)

enum {
        PROP_0,
        PROP_IMAGES,
        N_PROPS
};


GrImagePage *
gr_image_page_new (void)
{
        return g_object_new (GR_TYPE_IMAGE_PAGE, NULL);
}

static void
remove_hide_timeout (GrImagePage *page)
{
       if (page->hide_timeout != 0) {
                g_source_remove (page->hide_timeout);
                page->hide_timeout = 0;
       }
}

static void
gr_image_page_finalize (GObject *object)
{
        GrImagePage *page = GR_IMAGE_PAGE (object);

        remove_hide_timeout (page);
        g_clear_pointer (&page->images, g_array_unref);

        G_OBJECT_CLASS (gr_image_page_parent_class)->finalize (object);
}

static void
set_current_image (GrImagePage *page)
{
        if (page->images->len > page->index) {
                GrImage *ri = NULL;
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                GdkDisplay *display;
                GdkWindow *win;
                GdkMonitor *monitor;
                GdkRectangle geom;

                display = gtk_widget_get_display (GTK_WIDGET (page));
                win = gtk_widget_get_window (gtk_widget_get_toplevel (GTK_WIDGET (page)));
                monitor = gdk_display_get_monitor_at_window (display, win);
                gdk_monitor_get_geometry (monitor, &geom);

                ri = &g_array_index (page->images, GrImage, page->index);
                pixbuf = load_pixbuf_fit_size (ri->path, 0, geom.width - 80, geom.height - 80, FALSE);
                gtk_image_set_from_pixbuf (GTK_IMAGE (page->image), pixbuf);
        }
}

static void
stop_viewing (GrImagePage *page)
{
        GtkWidget *window;

        window = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_APPLICATION_WINDOW);
        gr_window_show_image (GR_WINDOW (window), NULL, -1);
}

static void
prev_image (GrImagePage *page)
{
        page->index = (page->index + page->images->len - 1) % page->images->len;
        set_current_image (page);
}

static void
next_image (GrImagePage *page)
{
        page->index = (page->index + 1) % page->images->len;
        set_current_image (page);
}

static void
show_buttons (GrImagePage *page)
{
        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (page->close_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->close_revealer), TRUE);

        if (!page->images || page->images->len < 2)
                return;

        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (page->next_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->next_revealer), TRUE);

        if (!gtk_revealer_get_child_revealed (GTK_REVEALER (page->prev_revealer)))
                        gtk_revealer_set_reveal_child (GTK_REVEALER (page->prev_revealer), TRUE);
}

static void
hide_buttons (GrImagePage *page)
{
        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->close_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->close_revealer), FALSE);

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->next_revealer)))
                gtk_revealer_set_reveal_child (GTK_REVEALER (page->next_revealer), FALSE);

        if (gtk_revealer_get_child_revealed (GTK_REVEALER (page->prev_revealer)))
                        gtk_revealer_set_reveal_child (GTK_REVEALER (page->prev_revealer), FALSE);
}

static gboolean
hide_timeout (gpointer data)
{
        GrImagePage *page = data;

        hide_buttons (page);

        page->hide_timeout = 0;

        return G_SOURCE_REMOVE;
}

static void
reset_hide_timeout (GrImagePage *page)
{
        remove_hide_timeout (page);
        page->hide_timeout = g_timeout_add (5000, hide_timeout, page);
}

static gboolean
motion_notify (GtkWidget   *widget,
               GdkEvent    *event,
               GrImagePage *page)
{
        show_buttons (page);
        reset_hide_timeout (page);

        return FALSE;
}

static gboolean
key_press_event (GtkWidget     *widget,
                 GdkEvent      *event,
                 GrImagePage   *page)
{
        GdkEventKey *key = (GdkEventKey *)event;

        if (key->keyval == GDK_KEY_space) {
                show_buttons (page);
                return TRUE;
        }
        if (key->keyval == GDK_KEY_Escape) {
                stop_viewing (page);
                return TRUE;
        }
        else if (key->keyval == GDK_KEY_Left) {
                prev_image (page);
                return TRUE;
        }
        else if (key->keyval == GDK_KEY_Right) {
                next_image (page);
                return TRUE;
        }

        return FALSE;
}

static void
gr_image_page_init (GrImagePage *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_POINTER_MOTION_MASK);
        gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_BUTTON_PRESS_MASK);
g_signal_connect (self->event_box, "motion-notify-event", G_CALLBACK (motion_notify), self);
        g_signal_connect (self->event_box, "key-press-event", G_CALLBACK (key_press_event), self);

        self->images = gr_image_array_new ();
}

static void
gr_image_page_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        GrImagePage *self = GR_IMAGE_PAGE (object);

        switch (prop_id)
          {
          case PROP_IMAGES:
                  g_value_set_boxed (value, self->images);
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_image_page_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        GrImagePage *self = GR_IMAGE_PAGE (object);

        switch (prop_id)
          {
          case PROP_IMAGES:
                  gr_image_page_set_images (self, (GArray *) g_value_get_boxed (value));
                  break;

          default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gr_image_page_class_init (GrImagePageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = gr_image_page_finalize;
        object_class->get_property = gr_image_page_get_property;
        object_class->set_property = gr_image_page_set_property;

        pspec = g_param_spec_boxed ("images", NULL, NULL,
                                    G_TYPE_ARRAY,
                                    G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_IMAGES, pspec);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-image-page.ui");
        gtk_widget_class_bind_template_child (widget_class, GrImagePage, image);
        gtk_widget_class_bind_template_child (widget_class, GrImagePage, event_box);
        gtk_widget_class_bind_template_child (widget_class, GrImagePage, prev_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrImagePage, next_revealer);
        gtk_widget_class_bind_template_child (widget_class, GrImagePage, close_revealer);

        gtk_widget_class_bind_template_callback (widget_class, stop_viewing);
        gtk_widget_class_bind_template_callback (widget_class, prev_image);
        gtk_widget_class_bind_template_callback (widget_class, next_image);
}

static void
add_image (GrImagePage    *page,
           GrImage *ri,
           gboolean        select)
{
        g_array_append_vals (page->images, ri, 1);
        ri = &g_array_index (page->images, GrImage, page->images->len - 1);
        ri->path = g_strdup (ri->path);

        if (select)
                page->index = page->images->len - 1;
        set_current_image (page);

        g_object_notify (G_OBJECT (page), "images");
}

void
gr_image_page_set_images (GrImagePage *page,
                          GArray      *images)
{
        int i;

        g_object_freeze_notify (G_OBJECT (page));

        g_array_remove_range (page->images, 0, page->images->len);
        g_object_notify (G_OBJECT (page), "images");

        for (i = 0; i < images->len; i++) {
                GrImage *ri = &g_array_index (images, GrImage, i);
                add_image (page, ri, FALSE);
        }

        page->index = 0;
        set_current_image (page);

        g_object_thaw_notify (G_OBJECT (page));
}

void
gr_image_page_show_image (GrImagePage *page,
                          int          idx)
{
        if (page->images->len > 0) {
                page->index = idx % page->images->len;
                set_current_image (page);
        }
}
