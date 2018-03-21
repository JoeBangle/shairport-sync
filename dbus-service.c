#include <stdio.h>
#include <string.h>

#include "config.h"

#include "common.h"
#include "player.h"
#include "rtsp.h"

#include "rtp.h"

#include "dacp.h"
#include "metadata_hub.h"

#include "dbus-service.h"

ShairportSyncDiagnostics *shairportSyncDiagnosticsSkeleton;
ShairportSyncRemoteControl *shairportSyncRemoteControlSkeleton;
ShairportSyncAdvancedRemoteControl *shairportSyncAdvancedRemoteControlSkeleton;

void dbus_metadata_watcher(struct metadata_bundle *argc, __attribute__((unused)) void *userdata) {
  // debug(1, "DBUS metadata watcher called");
  shairport_sync_advanced_remote_control_set_volume(shairportSyncAdvancedRemoteControlSkeleton,
                                                    argc->speaker_volume);

  // debug(1, "No diagnostics watcher required");

  // debug(1, "DBUS remote control watcher called");

  shairport_sync_remote_control_set_airplay_volume(shairportSyncRemoteControlSkeleton,
                                                   argc->airplay_volume);

  if (argc->dacp_server_active)
    shairport_sync_remote_control_set_server(shairportSyncRemoteControlSkeleton, argc->client_ip);
  else
    shairport_sync_remote_control_set_server(shairportSyncRemoteControlSkeleton, "");

  GVariantBuilder *dict_builder, *aa;

  /* Build the metadata array */
  // debug(1,"Build metadata");
  dict_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

  // Make up the artwork URI if we have one
  if (argc->cover_art_pathname) {
    char artURIstring[1024];
    sprintf(artURIstring, "file://%s", argc->cover_art_pathname);
    // sprintf(artURIstring,"");
    // debug(1,"artURI String: \"%s\".",artURIstring);
    GVariant *artUrl = g_variant_new("s", artURIstring);
    g_variant_builder_add(dict_builder, "{sv}", "mpris:artUrl", artUrl);
  }

  // Add the TrackID if we have one
  if (argc->item_id) {
    char trackidstring[128];
    // debug(1, "Set ID using mper ID: \"%u\".",argc->item_id);
    sprintf(trackidstring, "/org/gnome/ShairportSync/mper_%u", argc->item_id);
    GVariant *trackid = g_variant_new("o", trackidstring);
    g_variant_builder_add(dict_builder, "{sv}", "mpris:trackid", trackid);
  }

  // Add the track name if there is one
  if (argc->track_name) {
    // debug(1, "Track name set to \"%s\".", argc->track_name);
    GVariant *trackname = g_variant_new("s", argc->track_name);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:title", trackname);
  }

  // Add the album name if there is one
  if (argc->album_name) {
    // debug(1, "Album name set to \"%s\".", argc->album_name);
    GVariant *albumname = g_variant_new("s", argc->album_name);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:album", albumname);
  }

  // Add the artists if there are any (actually there will be at most one, but put it in an array)
  if (argc->artist_name) {
    /* Build the artists array */
    // debug(1,"Build artist array");
    aa = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_variant_builder_add(aa, "s", argc->artist_name);
    GVariant *artists = g_variant_builder_end(aa);
    g_variant_builder_unref(aa);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:artist", artists);
  }

  // Add the genres if there are any (actually there will be at most one, but put it in an array)
  if (argc->genre) {
    // debug(1,"Build genre");
    aa = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_variant_builder_add(aa, "s", argc->genre);
    GVariant *genres = g_variant_builder_end(aa);
    g_variant_builder_unref(aa);
    g_variant_builder_add(dict_builder, "{sv}", "xesam:genre", genres);
  }

  GVariant *dict = g_variant_builder_end(dict_builder);
  g_variant_builder_unref(dict_builder);

  // debug(1,"Set metadata");
  shairport_sync_remote_control_set_metadata(shairportSyncRemoteControlSkeleton, dict);
}

static gboolean on_handle_set_volume(ShairportSyncAdvancedRemoteControl *skeleton,
                                     GDBusMethodInvocation *invocation, const gint volume,
                                     __attribute__((unused)) gpointer user_data) {
  debug(1, "Set volume to %d.", volume);
  dacp_set_volume(volume);
  shairport_sync_advanced_remote_control_complete_set_volume(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_fast_forward(ShairportSyncRemoteControl *skeleton,
                                       GDBusMethodInvocation *invocation,
                                       __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("beginff");
  shairport_sync_remote_control_complete_fast_forward(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_rewind(ShairportSyncRemoteControl *skeleton,
                                 GDBusMethodInvocation *invocation,
                                 __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("beginrew");
  shairport_sync_remote_control_complete_rewind(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_toggle_mute(ShairportSyncRemoteControl *skeleton,
                                      GDBusMethodInvocation *invocation,
                                      __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("mutetoggle");
  shairport_sync_remote_control_complete_toggle_mute(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_next(ShairportSyncRemoteControl *skeleton,
                               GDBusMethodInvocation *invocation,
                               __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("nextitem");
  shairport_sync_remote_control_complete_next(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_previous(ShairportSyncRemoteControl *skeleton,
                                   GDBusMethodInvocation *invocation,
                                   __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("previtem");
  shairport_sync_remote_control_complete_previous(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_pause(ShairportSyncRemoteControl *skeleton,
                                GDBusMethodInvocation *invocation,
                                __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("pause");
  shairport_sync_remote_control_complete_pause(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_play_pause(ShairportSyncRemoteControl *skeleton,
                                     GDBusMethodInvocation *invocation,
                                     __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("playpause");
  shairport_sync_remote_control_complete_play_pause(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_play(ShairportSyncRemoteControl *skeleton,
                               GDBusMethodInvocation *invocation,
                               __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("play");
  shairport_sync_remote_control_complete_play(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_stop(ShairportSyncRemoteControl *skeleton,
                               GDBusMethodInvocation *invocation,
                               __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("stop");
  shairport_sync_remote_control_complete_stop(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_resume(ShairportSyncRemoteControl *skeleton,
                                 GDBusMethodInvocation *invocation,
                                 __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("playresume");
  shairport_sync_remote_control_complete_resume(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_shuffle_songs(ShairportSyncRemoteControl *skeleton,
                                        GDBusMethodInvocation *invocation,
                                        __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("shuffle_songs");
  shairport_sync_remote_control_complete_shuffle_songs(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_volume_up(ShairportSyncRemoteControl *skeleton,
                                    GDBusMethodInvocation *invocation,
                                    __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("volumeup");
  shairport_sync_remote_control_complete_volume_up(skeleton, invocation);
  return TRUE;
}

static gboolean on_handle_volume_down(ShairportSyncRemoteControl *skeleton,
                                      GDBusMethodInvocation *invocation,
                                      __attribute__((unused)) gpointer user_data) {
  send_simple_dacp_command("volumedown");
  shairport_sync_remote_control_complete_volume_down(skeleton, invocation);
  return TRUE;
}

gboolean notify_elapsed_time_callback(ShairportSyncDiagnostics *skeleton,
                                      __attribute__((unused)) gpointer user_data) {
  // debug(1, "\"notify_elapsed_time_callback\" called.");
  if (shairport_sync_diagnostics_get_elapsed_time(skeleton)) {
    config.debugger_show_elapsed_time = 1;
    debug(1, ">> start including elapsed time in logs");
  } else {
    config.debugger_show_elapsed_time = 0;
    debug(1, ">> stop including elapsed time in logs");
  }
  return TRUE;
}

gboolean notify_delta_time_callback(ShairportSyncDiagnostics *skeleton,
                                    __attribute__((unused)) gpointer user_data) {
  // debug(1, "\"notify_delta_time_callback\" called.");
  if (shairport_sync_diagnostics_get_delta_time(skeleton)) {
    config.debugger_show_relative_time = 1;
    debug(1, ">> start including delta time in logs");
  } else {
    config.debugger_show_relative_time = 0;
    debug(1, ">> stop including delta time in logs");
  }
  return TRUE;
}

gboolean notify_statistics_callback(ShairportSyncDiagnostics *skeleton,
                                    __attribute__((unused)) gpointer user_data) {
  // debug(1, "\"notify_statistics_callback\" called.");
  if (shairport_sync_diagnostics_get_statistics(skeleton)) {
    debug(1, ">> start logging statistics");
    config.statistics_requested = 1;
  } else {
    debug(1, ">> stop logging statistics");
    config.statistics_requested = 0;
  }
  return TRUE;
}

gboolean notify_verbosity_callback(ShairportSyncDiagnostics *skeleton,
                                   __attribute__((unused)) gpointer user_data) {
  gint th = shairport_sync_diagnostics_get_verbosity(skeleton);
  if ((th >= 0) && (th <= 3)) {
    if (th == 0)
      debug(1, ">> log verbosity set to %d.", th);
    debuglev = th;
    debug(1, ">> log verbosity set to %d.", th);
  } else {
    debug(1, ">> invalid log verbosity: %d. Ignored.", th);
  }
  return TRUE;
}

gboolean notify_loudness_filter_active_callback(ShairportSync *skeleton,
                                                __attribute__((unused)) gpointer user_data) {
  debug(1, "\"notify_loudness_filter_active_callback\" called.");
  if (shairport_sync_get_loudness_filter_active(skeleton)) {
    debug(1, "activating loudness filter");
    config.loudness = 1;
  } else {
    debug(1, "deactivating loudness filter");
    config.loudness = 0;
  }
  return TRUE;
}

gboolean notify_loudness_threshold_callback(ShairportSync *skeleton,
                                            __attribute__((unused)) gpointer user_data) {
  gdouble th = shairport_sync_get_loudness_threshold(skeleton);
  if ((th <= 0.0) && (th >= -100.0)) {
    debug(1, "Setting loudness threshhold to %f.", th);
    config.loudness_reference_volume_db = th;
  } else {
    debug(1, "Invalid loudness threshhold: %f. Ignored.", th);
  }
  return TRUE;
}

static gboolean on_handle_remote_command(ShairportSync *skeleton, GDBusMethodInvocation *invocation,
                                         const gchar *command,
                                         __attribute__((unused)) gpointer user_data) {
  debug(1, "RemoteCommand with command \"%s\".", command);
  send_simple_dacp_command((const char *)command);
  shairport_sync_complete_remote_command(skeleton, invocation);
  return TRUE;
}

static void on_dbus_name_acquired(GDBusConnection *connection, const gchar *name,
                                  __attribute__((unused)) gpointer user_data) {

  // debug(1, "Shairport Sync native D-Bus interface \"%s\" acquired on the %s bus.", name,
  // (config.dbus_service_bus_type == DBT_session) ? "session" : "system");
  shairportSyncSkeleton = shairport_sync_skeleton_new();

  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(shairportSyncSkeleton), connection,
                                   "/org/gnome/ShairportSync", NULL);

  shairport_sync_set_loudness_threshold(SHAIRPORT_SYNC(shairportSyncSkeleton),
                                        config.loudness_reference_volume_db);
  debug(1, "Loudness threshold is %f.", config.loudness_reference_volume_db);

  if (config.loudness == 0) {
    shairport_sync_set_loudness_filter_active(SHAIRPORT_SYNC(shairportSyncSkeleton), FALSE);
    debug(1, "Loudness is off");
  } else {
    shairport_sync_set_loudness_filter_active(SHAIRPORT_SYNC(shairportSyncSkeleton), TRUE);
    debug(1, "Loudness is on");
  }

  g_signal_connect(shairportSyncSkeleton, "notify::loudness-filter-active",
                   G_CALLBACK(notify_loudness_filter_active_callback), NULL);
  g_signal_connect(shairportSyncSkeleton, "notify::loudness-threshold",
                   G_CALLBACK(notify_loudness_threshold_callback), NULL);
  g_signal_connect(shairportSyncSkeleton, "handle-remote-command",
                   G_CALLBACK(on_handle_remote_command), NULL);

  // debug(1,"dbus_diagnostics_on_dbus_name_acquired");
  shairportSyncDiagnosticsSkeleton = shairport_sync_diagnostics_skeleton_new();
  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(shairportSyncDiagnosticsSkeleton),
                                   connection, "/org/gnome/ShairportSync", NULL);

  shairport_sync_diagnostics_set_verbosity(
      SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), debuglev);

  // debug(2,">> log verbosity is %d.",debuglev);

  if (config.statistics_requested == 0) {
    shairport_sync_diagnostics_set_statistics(
        SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), FALSE);
    // debug(1, ">> statistics logging is off");
  } else {
    shairport_sync_diagnostics_set_statistics(
        SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), TRUE);
    // debug(1, ">> statistics logging is on");
  }

  if (config.debugger_show_elapsed_time == 0) {
    shairport_sync_diagnostics_set_elapsed_time(
        SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), FALSE);
    // debug(1, ">> elapsed time is included in log entries");
  } else {
    shairport_sync_diagnostics_set_elapsed_time(
        SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), TRUE);
    // debug(1, ">> elapsed time is not included in log entries");
  }

  if (config.debugger_show_relative_time == 0) {
    shairport_sync_diagnostics_set_delta_time(
        SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), FALSE);
    // debug(1, ">> delta time is included in log entries");
  } else {
    shairport_sync_diagnostics_set_delta_time(
        SHAIRPORT_SYNC_DIAGNOSTICS(shairportSyncDiagnosticsSkeleton), TRUE);
    // debug(1, ">> delta time is not included in log entries");
  }

  g_signal_connect(shairportSyncDiagnosticsSkeleton, "notify::verbosity",
                   G_CALLBACK(notify_verbosity_callback), NULL);

  g_signal_connect(shairportSyncDiagnosticsSkeleton, "notify::statistics",
                   G_CALLBACK(notify_statistics_callback), NULL);

  g_signal_connect(shairportSyncDiagnosticsSkeleton, "notify::elapsed-time",
                   G_CALLBACK(notify_elapsed_time_callback), NULL);

  g_signal_connect(shairportSyncDiagnosticsSkeleton, "notify::delta-time",
                   G_CALLBACK(notify_delta_time_callback), NULL);

  // debug(1,"dbus_remote_control_on_dbus_name_acquired");
  shairportSyncRemoteControlSkeleton = shairport_sync_remote_control_skeleton_new();
  g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(shairportSyncRemoteControlSkeleton),
                                   connection, "/org/gnome/ShairportSync", NULL);

  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-fast-forward",
                   G_CALLBACK(on_handle_fast_forward), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-rewind",
                   G_CALLBACK(on_handle_rewind), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-toggle-mute",
                   G_CALLBACK(on_handle_toggle_mute), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-next", G_CALLBACK(on_handle_next),
                   NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-previous",
                   G_CALLBACK(on_handle_previous), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-pause", G_CALLBACK(on_handle_pause),
                   NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-play-pause",
                   G_CALLBACK(on_handle_play_pause), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-play", G_CALLBACK(on_handle_play),
                   NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-stop", G_CALLBACK(on_handle_stop),
                   NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-resume",
                   G_CALLBACK(on_handle_resume), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-shuffle-songs",
                   G_CALLBACK(on_handle_shuffle_songs), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-volume-up",
                   G_CALLBACK(on_handle_volume_up), NULL);
  g_signal_connect(shairportSyncRemoteControlSkeleton, "handle-volume-down",
                   G_CALLBACK(on_handle_volume_down), NULL);

  // debug(1,"dbus_advanced_remote_control_on_dbus_name_acquired");
  shairportSyncAdvancedRemoteControlSkeleton =
      shairport_sync_advanced_remote_control_skeleton_new();

  g_dbus_interface_skeleton_export(
      G_DBUS_INTERFACE_SKELETON(shairportSyncAdvancedRemoteControlSkeleton), connection,
      "/org/gnome/ShairportSync", NULL);

  g_signal_connect(shairportSyncAdvancedRemoteControlSkeleton, "handle-set-volume",
                   G_CALLBACK(on_handle_set_volume), NULL);

  add_metadata_watcher(dbus_metadata_watcher, NULL);

#ifdef HAVE_DBUS_REMOTE_CONTROL
  dbus_remote_control_on_dbus_name_acquired(connection, name, user_data);
#endif

  debug(1, "Shairport Sync native D-Bus service started at \"%s\" on the %s bus.", name,
        (config.dbus_service_bus_type == DBT_session) ? "session" : "system");
}

static void on_dbus_name_lost_again(__attribute__((unused)) GDBusConnection *connection,
                                    __attribute__((unused)) const gchar *name,
                                    __attribute__((unused)) gpointer user_data) {
  warn("Could not acquire a Shairport Sync native D-Bus interface \"%s\" on the %s bus.", name,
       (config.dbus_service_bus_type == DBT_session) ? "session" : "system");
}

static void on_dbus_name_lost(__attribute__((unused)) GDBusConnection *connection,
                              __attribute__((unused)) const gchar *name,
                              __attribute__((unused)) gpointer user_data) {
  // debug(1, "Could not acquire a Shairport Sync native D-Bus interface \"%s\" on the %s bus --
  // will try adding the process "
  //         "number to the end of it.",
  //      name, (config.dbus_service_bus_type == DBT_session) ? "session" : "system");
  pid_t pid = getpid();
  char interface_name[256] = "";
  sprintf(interface_name, "org.gnome.ShairportSync.i%d", pid);
  GBusType dbus_bus_type = G_BUS_TYPE_SYSTEM;
  if (config.dbus_service_bus_type == DBT_session)
    dbus_bus_type = G_BUS_TYPE_SESSION;
  // debug(1, "Looking for a Shairport Sync native D-Bus interface \"%s\" on the %s bus.",
  // interface_name,(config.dbus_service_bus_type == DBT_session) ? "session" : "system");
  g_bus_own_name(dbus_bus_type, interface_name, G_BUS_NAME_OWNER_FLAGS_NONE, NULL,
                 on_dbus_name_acquired, on_dbus_name_lost_again, NULL, NULL);
}

int start_dbus_service() {
  //  shairportSyncSkeleton = NULL;
  GBusType dbus_bus_type = G_BUS_TYPE_SYSTEM;
  if (config.dbus_service_bus_type == DBT_session)
    dbus_bus_type = G_BUS_TYPE_SESSION;
  // debug(1, "Looking for a Shairport Sync native D-Bus interface \"org.gnome.ShairportSync\" on
  // the %s bus.",(config.dbus_service_bus_type == DBT_session) ? "session" : "system");
  g_bus_own_name(dbus_bus_type, "org.gnome.ShairportSync", G_BUS_NAME_OWNER_FLAGS_NONE, NULL,
                 on_dbus_name_acquired, on_dbus_name_lost, NULL, NULL);
  return 0; // this is just to quieten a compiler warning
}
