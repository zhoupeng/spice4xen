/*
 * qxl local rendering (aka display on sdl/vnc)
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "qxl.h"

//skylark
#include "qemu-common.h"
#include "cursor.h"
#include "cpu-all.h"

static void qxl_flip(PCIQXLDevice *qxl, QXLRect *rect)
{
    uint8_t *src = qxl->guest_primary.data;
    uint8_t *dst = qxl->guest_primary.flipped;
    int len, i;

    src += (qxl->guest_primary.surface.height - rect->top - 1) *
        qxl->guest_primary.stride;
    dst += rect->top  * qxl->guest_primary.stride;
    src += rect->left * qxl->guest_primary.bytes_pp;
    dst += rect->left * qxl->guest_primary.bytes_pp;
    len  = (rect->right - rect->left) * qxl->guest_primary.bytes_pp;

    for (i = rect->top; i < rect->bottom; i++) {
        memcpy(dst, src, len);
        dst += qxl->guest_primary.stride;
        src -= qxl->guest_primary.stride;
    }
}

void qxl_render_resize(PCIQXLDevice *qxl)
{
    QXLSurfaceCreate *sc = &qxl->guest_primary.surface;

    qxl->guest_primary.stride = sc->stride;
    qxl->guest_primary.resized++;
    switch (sc->format) {
    case SPICE_SURFACE_FMT_16_555:
        qxl->guest_primary.bytes_pp = 2;
        qxl->guest_primary.bits_pp = 15;
        break;
    case SPICE_SURFACE_FMT_16_565:
        qxl->guest_primary.bytes_pp = 2;
        qxl->guest_primary.bits_pp = 16;
        break;
    case SPICE_SURFACE_FMT_32_xRGB:
    case SPICE_SURFACE_FMT_32_ARGB:
        qxl->guest_primary.bytes_pp = 4;
        qxl->guest_primary.bits_pp = 32;
        break;
    default:
        fprintf(stderr, "%s: unhandled format: %x\n", __FUNCTION__,
                qxl->guest_primary.surface.format);
        qxl->guest_primary.bytes_pp = 4;
        qxl->guest_primary.bits_pp = 32;
        break;
    }
}


void qxl_render_update(PCIQXLDevice *qxl)
{
    VGACommonState *vga = &qxl->vga;
    QXLRect dirty[32], update;
    void *ptr;
    int i;

    if (qxl->guest_primary.resized) {
        qxl->guest_primary.resized = 0;

        if (qxl->guest_primary.flipped) {
            qemu_free(qxl->guest_primary.flipped);
            qxl->guest_primary.flipped = NULL;
        }
        qemu_free_displaysurface(vga->ds);

        qxl->guest_primary.data = qemu_get_ram_ptr(qxl->vga.vram_offset);
        if (qxl->guest_primary.stride < 0) {
            // spice surface is upside down -> need extra buffer to flip 
            qxl->guest_primary.stride = -qxl->guest_primary.stride;
            qxl->guest_primary.flipped = qemu_malloc(qxl->guest_primary.surface.width *
                                                     qxl->guest_primary.stride);
            ptr = qxl->guest_primary.flipped;
        } else {
            ptr = qxl->guest_primary.data;
        }
        dprint(qxl, 1, "%s: %dx%d, stride %d, bpp %d, depth %d, flip %s\n",
               __FUNCTION__,
               qxl->guest_primary.surface.width,
               qxl->guest_primary.surface.height,
               qxl->guest_primary.stride,
               qxl->guest_primary.bytes_pp,
               qxl->guest_primary.bits_pp,
               qxl->guest_primary.flipped ? "yes" : "no");
        vga->ds->surface =
            qemu_create_displaysurface_from(qxl->guest_primary.surface.width,
                                            qxl->guest_primary.surface.height,
                                            qxl->guest_primary.bits_pp,
                                            qxl->guest_primary.stride,
                                            ptr);
        dpy_resize(vga->ds);
    }

    if (!qxl->guest_primary.commands) {
        return;
    }
    qxl->guest_primary.commands = 0;

    update.left   = 0;
    update.right  = qxl->guest_primary.surface.width;
    update.top    = 0;
    update.bottom = qxl->guest_primary.surface.height;

    memset(dirty, 0, sizeof(dirty));
    qxl->ssd.worker->update_area(qxl->ssd.worker, 0, &update,
                                 dirty, ARRAY_SIZE(dirty), 1);

    for (i = 0; i < ARRAY_SIZE(dirty); i++) {
        if (qemu_spice_rect_is_empty(dirty+i)) {
            break;
        }
        if (qxl->guest_primary.flipped) {
            qxl_flip(qxl, dirty+i);
        }
        dpy_update(vga->ds,
                   dirty[i].left, dirty[i].top,
                   dirty[i].right - dirty[i].left,
                   dirty[i].bottom - dirty[i].top);
    }
}

static QEMUCursor *qxl_cursor(PCIQXLDevice *qxl, QXLCursor *cursor)
{
    QEMUCursor *c;
    uint8_t *image, *mask;
    int size;
    c = cursor_alloc(cursor->header.width, cursor->header.height);
    c->hot_x = cursor->header.hot_spot_x;
    c->hot_y = cursor->header.hot_spot_y;
    switch (cursor->header.type) {
    case SPICE_CURSOR_TYPE_ALPHA:
        size = cursor->header.width * cursor->header.height * sizeof(uint32_t);
        memcpy(c->data, cursor->chunk.data, size);
        if (qxl->debug > 2) {
            cursor_print_ascii_art(c, "qxl/alpha");
        }
        break;
    case SPICE_CURSOR_TYPE_MONO:
        mask  = cursor->chunk.data;
        image = mask + cursor_get_mono_bpl(c) * c->width;
        cursor_set_mono(c, 0xffffff, 0x000000, image, 1, mask);
        if (qxl->debug > 2) {
            cursor_print_ascii_art(c, "qxl/mono");
        }
        break;
    default:
        fprintf(stderr, "%s: not implemented: type %d\n",
                __FUNCTION__, cursor->header.type);
        goto fail;
    }
    return c;

fail:
    cursor_put(c);
    return NULL;
}


/* called from spice server thread context only */
void qxl_render_cursor(PCIQXLDevice *qxl, QXLCommandExt *ext)
{
    QXLCursorCmd *cmd = qxl_phys2virt(qxl, ext->cmd.data, ext->group_id);
    QXLCursor *cursor;
    QEMUCursor *c;
    int x = -1, y = -1;

    if (!qxl->ssd.ds->mouse_set || !qxl->ssd.ds->cursor_define) {
        return;
    }

    if (qxl->debug > 1 && cmd->type != QXL_CURSOR_MOVE) {
        fprintf(stderr, "%s", __FUNCTION__);
        qxl_log_cmd_cursor(qxl, cmd, ext->group_id);
        fprintf(stderr, "\n");
    }
    switch (cmd->type) {
    case QXL_CURSOR_SET:
        x = cmd->u.set.position.x;
        y = cmd->u.set.position.y;
        cursor = qxl_phys2virt(qxl, cmd->u.set.shape, ext->group_id);
        if (cursor->chunk.data_size != cursor->data_size) {
            fprintf(stderr, "%s: multiple chunks\n", __FUNCTION__);
            return;
        }
        c = qxl_cursor(qxl, cursor);
        if (c == NULL) {
            c = cursor_builtin_left_ptr();
        }
        qemu_mutex_lock_iothread();
        qxl->ssd.ds->cursor_define(c);
        qxl->ssd.ds->mouse_set(x, y, 1);
        qemu_mutex_unlock_iothread();
        cursor_put(c);
        break;
    case QXL_CURSOR_MOVE:
        x = cmd->u.position.x;
        y = cmd->u.position.y;
        qemu_mutex_lock_iothread();
        qxl->ssd.ds->mouse_set(x, y, 1);
        qemu_mutex_unlock_iothread();
        break;
    }
}
