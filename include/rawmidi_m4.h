#ifdef __cplusplus
extern "C" {
#endif

size_t snd_rawmidi_params_sizeof();
#define snd_rawmidi_params_alloca(ptr) ({assert(ptr); *ptr = (snd_rawmidi_params_t *) alloca(snd_rawmidi_params_sizeof()); 0;})
int snd_rawmidi_params_malloc(snd_rawmidi_params_t **ptr);
void snd_rawmidi_params_free(snd_rawmidi_params_t *obj);
void snd_rawmidi_params_copy(snd_rawmidi_params_t *dst, const snd_rawmidi_params_t *src);

int snd_rawmidi_params_set_buffer_size(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params, size_t val);
size_t snd_rawmidi_params_get_buffer_size(const snd_rawmidi_params_t *params);

int snd_rawmidi_params_set_avail_min(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params, size_t val);
size_t snd_rawmidi_params_get_avail_min(const snd_rawmidi_params_t *params);

int snd_rawmidi_params_set_no_active_sensing(snd_rawmidi_t *rmidi, snd_rawmidi_params_t *params, int val);
int snd_rawmidi_params_get_no_active_sensing(const snd_rawmidi_params_t *params);

size_t snd_rawmidi_info_sizeof();
#define snd_rawmidi_info_alloca(ptr) ({assert(ptr); *ptr = (snd_rawmidi_info_t *) alloca(snd_rawmidi_info_sizeof()); 0;})
int snd_rawmidi_info_malloc(snd_rawmidi_info_t **ptr);
void snd_rawmidi_info_free(snd_rawmidi_info_t *obj);
void snd_rawmidi_info_copy(snd_rawmidi_info_t *dst, const snd_rawmidi_info_t *src);

unsigned int snd_rawmidi_info_get_device(const snd_rawmidi_info_t *obj);

unsigned int snd_rawmidi_info_get_subdevice(const snd_rawmidi_info_t *obj);

snd_rawmidi_stream_t snd_rawmidi_info_get_stream(const snd_rawmidi_info_t *obj);

int snd_rawmidi_info_get_card(const snd_rawmidi_info_t *obj);

unsigned int snd_rawmidi_info_get_flags(const snd_rawmidi_info_t *obj);

const char * snd_rawmidi_info_get_id(const snd_rawmidi_info_t *obj);

const char * snd_rawmidi_info_get_name(const snd_rawmidi_info_t *obj);

const char * snd_rawmidi_info_get_subdevice_name(const snd_rawmidi_info_t *obj);

unsigned int snd_rawmidi_info_get_subdevices_count(const snd_rawmidi_info_t *obj);

unsigned int snd_rawmidi_info_get_subdevices_avail(const snd_rawmidi_info_t *obj);

void snd_rawmidi_info_set_device(snd_rawmidi_info_t *obj, unsigned int val);

void snd_rawmidi_info_set_subdevice(snd_rawmidi_info_t *obj, unsigned int val);

void snd_rawmidi_info_set_stream(snd_rawmidi_info_t *obj, snd_rawmidi_stream_t val);

size_t snd_rawmidi_status_sizeof();
#define snd_rawmidi_status_alloca(ptr) ({assert(ptr); *ptr = (snd_rawmidi_status_t *) alloca(snd_rawmidi_status_sizeof()); 0;})
int snd_rawmidi_status_malloc(snd_rawmidi_status_t **ptr);
void snd_rawmidi_status_free(snd_rawmidi_status_t *obj);
void snd_rawmidi_status_copy(snd_rawmidi_status_t *dst, const snd_rawmidi_status_t *src);

void snd_rawmidi_status_get_tstamp(const snd_rawmidi_status_t *obj, snd_timestamp_t *ptr);

size_t snd_rawmidi_status_get_avail(const snd_rawmidi_status_t *obj);

size_t snd_rawmidi_status_get_avail_max(const snd_rawmidi_status_t *obj);


#ifdef __cplusplus
}
#endif
