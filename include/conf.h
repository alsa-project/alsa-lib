
enum _snd_config_type {
        SND_CONFIG_TYPE_INTEGER,
        SND_CONFIG_TYPE_REAL,
        SND_CONFIG_TYPE_STRING,
	SND_CONFIG_TYPE_COMPOUND,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_config_type *snd_config_type_t;
#else
typedef enum _snd_config_type snd_config_type_t;
#endif

#define SND_CONFIG_TYPE_INTEGER ((snd_config_type_t) SND_CONFIG_TYPE_INTEGER)
#define SND_CONFIG_TYPE_REAL ((snd_config_type_t) SND_CONFIG_TYPE_REAL)
#define SND_CONFIG_TYPE_STRING ((snd_config_type_t) SND_CONFIG_TYPE_STRING)
#define SND_CONFIG_TYPE_COMPOUND ((snd_config_type_t) SND_CONFIG_TYPE_COMPOUND)

typedef struct _snd_config snd_config_t;
typedef struct _snd_config_iterator *snd_config_iterator_t;

#ifdef __cplusplus
extern "C" {
#endif

snd_config_type_t snd_config_get_type(snd_config_t *config);
const char *snd_config_get_id(snd_config_t *config);

int snd_config_top(snd_config_t **config);

int snd_config_load(snd_config_t *config, snd_input_t *in);
int snd_config_save(snd_config_t *config, snd_output_t *out);

int snd_config_search(snd_config_t *config, const char *key,
		      snd_config_t **result);
int snd_config_searchv(snd_config_t *config, 
		       snd_config_t **result, ...);

int snd_config_add(snd_config_t *config, snd_config_t *leaf);
int snd_config_delete(snd_config_t *config);

int snd_config_make(snd_config_t **config, const char *key,
		    snd_config_type_t type);
int snd_config_make_integer(snd_config_t **config, const char *key);
int snd_config_make_real(snd_config_t **config, const char *key);
int snd_config_make_string(snd_config_t **config, const char *key);
int snd_config_make_compound(snd_config_t **config, const char *key, int join);

int snd_config_set_integer(snd_config_t *config, long value);
int snd_config_set_real(snd_config_t *config, double value);
int snd_config_set_string(snd_config_t *config, const char *value);
int snd_config_get_integer(snd_config_t *config, long *value);
int snd_config_get_real(snd_config_t *config, double *value);
int snd_config_get_string(snd_config_t *config, const char **value);

snd_config_iterator_t snd_config_iterator_first(snd_config_t *node);
snd_config_iterator_t snd_config_iterator_next(snd_config_iterator_t iterator);
snd_config_iterator_t snd_config_iterator_end(snd_config_t *node);
snd_config_t *snd_config_iterator_entry(snd_config_iterator_t iterator);

#define snd_config_for_each(pos, next, node) \
	for (pos = snd_config_iterator_first(node), next = snd_config_iterator_next(pos); pos != snd_config_iterator_end(node); pos = next, next = snd_config_iterator_next(pos))

snd_config_type_t snd_config_get_type(snd_config_t *config);
const char *snd_config_get_id(snd_config_t *config);

extern snd_config_t *snd_config;
int snd_config_update();

#ifdef __cplusplus
}
#endif

