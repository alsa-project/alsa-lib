#ifdef __cplusplus
extern "C" {
#endif

size_t snd_hwdep_info_sizeof();
#define snd_hwdep_info_alloca(ptr) ({assert(ptr); *ptr = (snd_hwdep_info_t *) alloca(snd_hwdep_info_sizeof()); 0;})
int snd_hwdep_info_malloc(snd_hwdep_info_t **ptr);
void snd_hwdep_info_free(snd_hwdep_info_t *obj);
void snd_hwdep_info_copy(snd_hwdep_info_t *dst, const snd_hwdep_info_t *src);

unsigned int snd_hwdep_info_get_device(const snd_hwdep_info_t *obj);

int snd_hwdep_info_get_card(const snd_hwdep_info_t *obj);

const char * snd_hwdep_info_get_id(const snd_hwdep_info_t *obj);

const char * snd_hwdep_info_get_name(const snd_hwdep_info_t *obj);

snd_hwdep_type_t snd_hwdep_info_get_type(const snd_hwdep_info_t *obj);

void snd_hwdep_info_set_device(snd_hwdep_info_t *obj, unsigned int val);


#ifdef __cplusplus
}
#endif
