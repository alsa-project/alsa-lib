#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "../include/asoundlib.h"

int main(int argc, char *argv[])
{
	char *str;
	snd_card_type_t type;
	int idx, err;
	
	for (idx = 1; idx < argc; idx++) {
		if (isdigit(argv[idx][0])) {
			type = (snd_card_type_t)atoi(argv[idx]);
			err = snd_card_type_enum_to_string(type, &str);
			printf("enum_to_string: input %i -> '%s', error %i\n", (int)type, str, err);
		} else {
			str = argv[idx];
			err = snd_card_type_string_to_enum(str, &type);
			printf("string_to_enum: input '%s' -> %i, error %i\n", str, (int)type, err);
		}
	}
	return EXIT_SUCCESS;
}
