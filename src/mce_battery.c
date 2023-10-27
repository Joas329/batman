/*
 * Copyright (C) 2019-2022 Jolla Ltd.
 * Copyright (C) 2019-2022 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include "mce_battery.h"
#include "mce_proxy.h"
#include "mce_log_p.h"
#include <batman/batman-wrappers.h>

#include <gutil_misc.h>

/* Generated headers */
#include "org.ayatana.indicator.power.Battery.h"

#define MCE_BATTERY_STATUS_OK "ok" //30% above
#define MCE_BATTERY_STATUS_LOW "low" //30%
#define MCE_BATTERY_STATUS_EMPTY "very_low" //15% under
#define MCE_BATTERY_STATUS_CRITICAL "critical" //5% under 
#define MCE_BATTERY_STATUS_UNKNOWN "unknown"

enum mce_battery_ind {
    BATTERY_IND_LEVEL,
    BATTERY_IND_STATUS,
    BATTERY_IND_COUNT
};

typedef enum mce_battery_flags {
    BATTERY_HAVE_NONE = 0x00,
    BATTERY_HAVE_LEVEL = 0x01,
    BATTERY_HAVE_STATUS = 0x02
} BATTERY_FLAGS;

#define BATTERY_HAVE_ALL (BATTERY_HAVE_LEVEL | BATTERY_HAVE_STATUS)

struct mce_battery_priv {
    MceProxy* proxy;
    BATTERY_FLAGS flags;
    gulong proxy_valid_id;
    gulong battery_ind_id;
};

enum mce_battery_signal {
    SIGNAL_VALID_CHANGED,
    SIGNAL_LEVEL_CHANGED,
    SIGNAL_STATUS_CHANGED,
    SIGNAL_COUNT
};

#define SIGNAL_VALID_CHANGED_NAME   "mce-battery-valid-changed"
#define SIGNAL_LEVEL_CHANGED_NAME   "mce-battery-level-changed"
#define SIGNAL_STATUS_CHANGED_NAME  "mce-battery-status-changed"

static guint mce_battery_signals[SIGNAL_COUNT] = { 0 };

typedef GObjectClass MceBatteryClass;
G_DEFINE_TYPE(MceBattery, mce_battery, G_TYPE_OBJECT)
#define PARENT_CLASS mce_battery_parent_class
#define MCE_BATTERY_TYPE (mce_battery_get_type())
#define MCE_BATTERY(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj,\
        MCE_BATTERY_TYPE,MceBattery))

/*==========================================================================*
 * Implementation
 *==========================================================================*/

// static
// void
// mce_battery_check_valid(
//     MceBattery* self)
// {
//     MceBatteryPriv* priv = self->priv;
//     const gboolean valid = priv->proxy->valid &&
//         (priv->flags & BATTERY_HAVE_ALL) == BATTERY_HAVE_ALL;

//     if (valid != self->valid) {
//         self->valid = valid;
//         g_signal_emit(self, mce_battery_signals[SIGNAL_VALID_CHANGED], 0);
//     }
// }

// static
// void
// mce_battery_level_update(
//     MceBattery* self,
//     gint level)
// {
//     MceBatteryPriv* priv = self->priv;
//     const guint new_level = (level < 0) ? 0 : (level > 100) ? 100 : level;

//     if (self->level != new_level) {
//         self->level = new_level;
//         g_signal_emit(self, mce_battery_signals[SIGNAL_LEVEL_CHANGED], 0);
//     }
//     priv->flags |= BATTERY_HAVE_LEVEL;
//     mce_battery_check_valid(self);
// }

static
void
mce_battery_status_update(
    MceBattery* self,
    const char* status)
{
    MceBatteryPriv* priv = self->priv;
    MCE_BATTERY_STATUS new_status;

    guint batteryLevel = self.level;

    if (batteryLevel >= 30 ) {
        new_status = MCE_BATTERY_OK;
    } else if (batteryLevel < 30 && batteryLevel >= 15) {
        new_status = MCE_BATTERY_LOW;
    } else if (batteryLevel < 15 && batteryLevel >= 5) {
        new_status = MCE_BATTERY_LOW;
    } else if (batteryLevel < 5 ) {
        new_status = MCE_BATTERY_EMPTY;
    } else {
        GASSERT(!g_strcmp0(status, MCE_BATTERY_STATUS_UNKNOWN));
        new_status = MCE_BATTERY_UNKNOWN;
    }
    if (self->status != new_status) {
        self->status = new_status;
        // g_signal_emit(self, mce_battery_signals[SIGNAL_STATUS_CHANGED], 0);
    }
    // mce_battery_check_valid(self);
}

// static void
// on_properties_changed (GDBusProxy          *proxy,
//                        GVariant            *changed_properties,
//                        const gchar* const  *invalidated_properties,
//                        gpointer             user_data)
// {
//   GVariantIter iter;
//   const gchar *key;
//   GVariant *value;

//   g_variant_iter_init (&iter, changed_properties);
//   while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
//     {
//         if (!g_strcmp0(key, "PowerPercentage")) {
//             gint level;
//             g_variant_get (value, "i", &level);
//             mce_battery_level_update(MCE_BATTERY(user_data), level);
//         } else if (!g_strcmp0(key, "PowerLevel")) {
//             const char* status;
//             g_variant_get (value, "s", &status);
//             mce_battery_status_update(MCE_BATTERY(user_data), status);
//         }

//       g_variant_unref (value);
//     }
// }

static
void
mce_battery_query(
    MceBattery* self)
{
    // MceBatteryPriv* priv = self->priv;
    // MceProxy* proxy = priv->proxy;
    UpClient *upower = up_client_new();
    gdouble battery_percentage;

    const gchar *statelabel = findBattery(upower, &battery_percentage);

    self->level = battery_percentage;
    /*
     * proxy->signal and proxy->request may not be available at the
     * time when MceBattery is created. In that case we have to wait
     * for the valid signal before we can connect the battery state
     * signal and submit the initial query.
     */
    // if (proxy->ayatana_request && !priv->battery_ind_id) {
    //     priv->battery_ind_id = g_signal_connect(proxy->ayatana_request, "g-properties-changed",
    //         G_CALLBACK(on_properties_changed), self);
    // }

    // if (proxy->ayatana_request && proxy->valid) {
    //     const char* status = org_ayatana_indicator_power_battery_get_power_level(proxy->ayatana_request);
    //     mce_battery_status_update(self, status);

    //     u_int32_t level = org_ayatana_indicator_power_battery_get_power_percentage(proxy->ayatana_request);
    //     mce_battery_level_update(self, level);
    // }
}

// static
// void
// mce_battery_valid_changed(
//     MceProxy* proxy,
//     void* arg)
// {
//     MceBattery* self = MCE_BATTERY(arg);
//     MceBatteryPriv* priv = self->priv;

//     if (proxy->valid) {
//         mce_battery_query(self);
//     } else {
//         priv->flags = BATTERY_HAVE_NONE;
//     }
//     mce_battery_check_valid(self);
// }

/*==========================================================================*
 * API
 *==========================================================================*/

MceBattery*
mce_battery_new()
{
    /* MCE assumes one battery */
    static MceBattery* mce_battery_instance = NULL;

    if (mce_battery_instance) {
        mce_battery_ref(mce_battery_instance);
    } else {
        mce_battery_instance = g_object_new(MCE_BATTERY_TYPE, NULL);
        mce_battery_query(mce_battery_instance);
        g_object_add_weak_pointer(G_OBJECT(mce_battery_instance),
            (gpointer*)(&mce_battery_instance));
    }
    return mce_battery_instance;
}

MceBattery*
mce_battery_ref(
    MceBattery* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(MCE_BATTERY(self));
    }
    return self;
}

void
mce_battery_unref(
    MceBattery* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(MCE_BATTERY(self));
    }
}

gulong
mce_battery_add_valid_changed_handler(
    MceBattery* self,
    MceBatteryFunc fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        SIGNAL_VALID_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
mce_battery_add_level_changed_handler(
    MceBattery* self,
    MceBatteryFunc fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        SIGNAL_LEVEL_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
mce_battery_add_status_changed_handler(
    MceBattery* self,
    MceBatteryFunc fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        SIGNAL_STATUS_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

void
mce_battery_remove_handler(
    MceBattery* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

void
mce_battery_remove_handlers(
    MceBattery* self,
    gulong* ids,
    guint count)
{
    gutil_disconnect_handlers(self, ids, count);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
mce_battery_init(
    MceBattery* self)
{
    MceBatteryPriv* priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MCE_BATTERY_TYPE,
        MceBatteryPriv);

    // self->priv = priv;
    // priv->proxy = mce_proxy_new();
    // priv->proxy_valid_id = mce_proxy_add_valid_changed_handler(priv->proxy,
    //     mce_battery_valid_changed, self);
}

static
void
mce_battery_finalize(
    GObject* object)
{
    MceBattery* self = MCE_BATTERY(object);
    MceBatteryPriv* priv = self->priv;

    // mce_proxy_remove_handler(priv->proxy, priv->battery_ind_id);
    // mce_proxy_remove_handler(priv->proxy, priv->proxy_valid_id);
    // mce_proxy_unref(priv->proxy);
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
mce_battery_class_init(
    MceBatteryClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = mce_battery_finalize;
    // g_type_class_add_private(klass, sizeof(MceBatteryPriv));
    mce_battery_signals[SIGNAL_VALID_CHANGED] =
        g_signal_new(SIGNAL_VALID_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    mce_battery_signals[SIGNAL_LEVEL_CHANGED] =
        g_signal_new(SIGNAL_LEVEL_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    mce_battery_signals[SIGNAL_STATUS_CHANGED] =
        g_signal_new(SIGNAL_STATUS_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
