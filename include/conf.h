
#define LIST_HEAD_IS_DEFINED
struct list_head {
        struct list_head *next, *prev;
};        

/**
 * list_entry - get the struct for this entry
 * @ptr:        the &struct list_head pointer.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
        ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

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

struct _snd_config {
	char *id;
	snd_config_type_t type;
	union {
		long integer;
		char *string;
		double real;
		struct {
			struct list_head fields;
			int join;
		} compound;
	} u;
	struct list_head list;
	snd_config_t *father;
};

static inline snd_config_type_t snd_config_type(snd_config_t *config)
{
	return config->type;
}

static inline char *snd_config_id(snd_config_t *config)
{
	return config->id;
}

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
int snd_config_integer_make(snd_config_t **config, const char *key);
int snd_config_real_make(snd_config_t **config, const char *key);
int snd_config_string_make(snd_config_t **config, const char *key);
int snd_config_compound_make(snd_config_t **config, const char *key, int join);

int snd_config_integer_set(snd_config_t *config, long value);
int snd_config_real_set(snd_config_t *config, double value);
int snd_config_string_set(snd_config_t *config, const char *value);
int snd_config_integer_get(snd_config_t *config, long *value);
int snd_config_real_get(snd_config_t *config, double *value);
int snd_config_string_get(snd_config_t *config, const char **value);

typedef struct list_head *snd_config_iterator_t;

#define snd_config_foreach(iterator, node) \
	assert((node)->type == SND_CONFIG_TYPE_COMPOUND); \
	for (iterator = (node)->u.compound.fields.next; iterator != &(node)->u.compound.fields; iterator = iterator->next)

#define snd_config_entry(iterator) list_entry(iterator, snd_config_t, list)

snd_config_type_t snd_config_type(snd_config_t *config);
char *snd_config_id(snd_config_t *config);

extern snd_config_t *snd_config;
int snd_config_update();
