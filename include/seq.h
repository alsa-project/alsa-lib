/****************************************************************************
 *                                                                          *
 *                                seq.h                                     *
 *                              Sequencer                                   *
 *                                                                          *
 ****************************************************************************/

#define SND_SEQ_OPEN_OUT	(O_WRONLY)
#define SND_SEQ_OPEN_IN		(O_RDONLY)
#define SND_SEQ_OPEN		(O_RDWR)

#ifdef __cplusplus
extern "C" {
#endif

int snd_seq_open(void **handle, int mode);
int snd_seq_close(void *handle);
int snd_seq_file_descriptor(void *handle);
int snd_seq_block_mode(void *handle, int enable);
int snd_seq_client_id(void *handle);
int snd_seq_system_info(void *handle, snd_seq_system_info_t *info);
int snd_seq_get_client_info(void *handle, snd_seq_client_info_t *info);
int snd_seq_get_any_client_info(void *handle, int client, snd_seq_client_info_t *info);
int snd_seq_set_client_info(void *handle, snd_seq_client_info_t *info);
int snd_seq_create_port(void *handle, snd_seq_port_info_t *info);
int snd_seq_delete_port(void *handle, snd_seq_port_info_t *info);
int snd_seq_get_port_info(void *handle, int port, snd_seq_port_info_t *info);
int snd_seq_get_any_port_info(void *handle, int client, int port, snd_seq_port_info_t *info);
int snd_seq_set_port_info(void *handle, int port, snd_seq_port_info_t *info);
int snd_seq_subscribe_port(void *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_unsubscribe_port(void *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_get_queue_info(void *handle, int q, snd_seq_queue_info_t *queue);
int snd_seq_set_queue_info(void *handle, int q, snd_seq_queue_info_t *queue);
int snd_seq_get_queue_client(void *handle, int q, snd_seq_queue_client_t *queue);
int snd_seq_set_queue_client(void *handle, int q, snd_seq_queue_client_t *queue);
int snd_seq_alloc_queue(void *handle, snd_seq_queue_info_t *queue);
int snd_seq_free_queue(void *handle, int q);
/* event routines */
snd_seq_event_t *snd_seq_create_event(void);
int snd_seq_free_event(snd_seq_event_t *ev);
int snd_seq_event_length(snd_seq_event_t *ev);
int snd_seq_event_output(void *handle, snd_seq_event_t *ev);
int snd_seq_event_input(void *handle, snd_seq_event_t **ev);
int snd_seq_flush_output(void *handle);
int snd_seq_drain_output(void *handle);
int snd_seq_drain_input(void *handle);
/* misc */
void snd_seq_set_bit(int nr, void *array);
int snd_seq_change_bit(int nr, void *array);
int snd_seq_get_bit(int nr, void *array);

#ifdef __cplusplus
}
#endif

