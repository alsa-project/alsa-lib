/****************************************************************************
 *                                                                          *
 *                               timer.h                                    *
 *                           Timer interface                                *
 *                                                                          *
 ****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

int snd_timer_open(void **handle);
int snd_timer_close(void *handle);
int snd_timer_file_descriptor(void *handle);
int snd_timer_general_info(void *handle, snd_timer_general_info_t * info);
int snd_timer_select(void *handle, snd_timer_select_t *tselect);
int snd_timer_info(void *handle, snd_timer_info_t *timer);
int snd_timer_params(void *handle, snd_timer_params_t *params);
int snd_timer_status(void *handle, snd_timer_status_t *status);
int snd_timer_start(void *handle);
int snd_timer_stop(void *handle);
int snd_timer_continue(void *handle);
ssize_t snd_timer_read(void *handle, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

