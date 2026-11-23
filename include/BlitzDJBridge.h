#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct blitzdj_handle blitzdj_handle;

blitzdj_handle* blitzdj_create(const char* client_id, const char* redirect_uri, const char* session_path);
void blitzdj_destroy(blitzdj_handle* handle);

const char* blitzdj_version(void);

char* blitzdj_authorization_url(blitzdj_handle* handle);
int blitzdj_handle_redirect(blitzdj_handle* handle, const char* redirect_url);
int blitzdj_is_authenticated(blitzdj_handle* handle);
void blitzdj_logout(blitzdj_handle* handle);

int blitzdj_play(blitzdj_handle* handle);
int blitzdj_pause(blitzdj_handle* handle);
int blitzdj_next(blitzdj_handle* handle);
int blitzdj_previous(blitzdj_handle* handle);
int blitzdj_set_volume(blitzdj_handle* handle, int volume);
int blitzdj_seek(blitzdj_handle* handle, int position_ms);
int blitzdj_set_shuffle(blitzdj_handle* handle, int enabled);

char* blitzdj_playback_json(blitzdj_handle* handle);
char* blitzdj_devices_json(blitzdj_handle* handle);
char* blitzdj_search_json(blitzdj_handle* handle, const char* query, int limit);
char* blitzdj_recommend_json(blitzdj_handle* handle, const char* query, int count);

int blitzdj_play_track(blitzdj_handle* handle, const char* track_uri);
int blitzdj_queue_track(blitzdj_handle* handle, const char* track_uri);
int blitzdj_transfer_playback(blitzdj_handle* handle, const char* device_id);

int blitzdj_start_auto_dj(blitzdj_handle* handle);
void blitzdj_stop_auto_dj(blitzdj_handle* handle);
int blitzdj_is_auto_dj_running(blitzdj_handle* handle);
void blitzdj_set_search_query(blitzdj_handle* handle, const char* query);
char* blitzdj_search_query(blitzdj_handle* handle);
int blitzdj_queue_recommendations(blitzdj_handle* handle, int count);
void blitzdj_add_favorite_artist(blitzdj_handle* handle, const char* artist);
void blitzdj_add_favorite_track(blitzdj_handle* handle, const char* track_id);

char* blitzdj_last_error(blitzdj_handle* handle);
void blitzdj_free_string(char* value);

#ifdef __cplusplus
}
#endif
