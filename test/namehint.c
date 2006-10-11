#include "../include/asoundlib.h"
#include <err.h>

int main(int argc, char *argv[])
{
	const char *iface = "pcm";
	snd_ctl_elem_iface_t niface;
	char **hints, **n;
	int err;

	if (argc > 1)
		iface = argv[1];
	for (niface = 0; niface < SND_CTL_ELEM_IFACE_LAST; niface++)
		if (strcmp(snd_ctl_iface_conf_name(niface), iface) == 0)
			break;
	if (niface > SND_CTL_ELEM_IFACE_LAST)
		errx(1, "interface %s dnoes not exist", iface);
	err = snd_device_name_hint(-1, niface, &hints);
	if (err < 0)
		errx(1, "snd_device_name_hint error: %s", snd_strerror(err));
	n = hints;
	while (*n != NULL) {
		printf("%s\n", *n);
		n++;
	}
	snd_device_name_free_hint(hints);
	return 0;
}
