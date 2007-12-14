#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <openssl/rand.h>
#include <openssl/md5.h>

/* When this is defined, the output is the key checker, not the key generator */
/* #undef MV_VERIFY_KEY_ONLY */

#define MV_DEMO_KEY	"metavize the demo"

static int
mv_verify_key(unsigned char *in_key)
{
    MD5_CTX	ctx = { 0 };
    unsigned char	random_bytes[4];
    unsigned char	md5_buffer[16];
    int		i = 0;

    memcpy(random_bytes, in_key, 4);
    
    MD5_Init(&ctx);
    MD5_Update(&ctx, random_bytes, 4);
    MD5_Update(&ctx, MV_DEMO_KEY, strlen(MV_DEMO_KEY));
    MD5_Final(md5_buffer, &ctx);

    // fold the first time
    for(i = 0; i < 8; i++) {
    	md5_buffer[i] ^= md5_buffer[i+8];
    }

    // fold the second time
    for(i = 0; i < 4; i++) {
    	md5_buffer[i] ^= md5_buffer[i+4];
    }

    // verify the result
    if(memcmp(md5_buffer, &(in_key[4]), 4) != 0) {
    	// key was not valid
    	return 1;
    }

    return 0;
}

static int
mv_get_binary_key(unsigned char *ascii_key, unsigned char *binary_key)
{
    int		i = 0, ak_idx;
    int		a, b, c;

    ak_idx = 0;
    for(i = 0; i < 8; i++) {
    	a = tolower(ascii_key[ak_idx++]);
	b = tolower(ascii_key[ak_idx++]);

	if(!((a >= '0') && (a <= '9'))  && !((a >= 'a') && (a <= 'f'))) {
	    fprintf(stderr, "Invalid characters in key.\n");
	    return -1;
	}
	    
	if(!((b >= '0') && (b <= '9'))  && !((b >= 'a') && (b <= 'f'))) {
	    fprintf(stderr, "Invalid characters in key.\n");
	    return -1;
	}

	a = (isalpha(a) ? ((a - 'a') + 10) : (a - '0'));
	b = (isalpha(b) ? ((b - 'a') + 10) : (b - '0'));
	    
	c = (a << 4) + b;
	if((c < 0) || (c > 255)) {
	    fprintf(stderr, "Math is hard, let's go shopping.\n");
	    return -1;
	}

	binary_key[i] = (unsigned char)c;
	if((i % 2) && (i != 7)) {
	    ak_idx++;
	}
    }

    return 0;
}

static int
mv_do_verify_key(unsigned char *ascii_key)
{
    unsigned char	binary_key[8];
    int			rval = 0;

    memset(binary_key, 0, 8);

    if(mv_get_binary_key(ascii_key, binary_key) != 0) {
    	return -1;
    }

    rval = mv_verify_key(binary_key);
    memset(binary_key, 0, 8);

    return rval;
}

#ifndef MV_VERIFY_KEY_ONLY

static int 
mv_get_printable_key(unsigned char *binary_key, unsigned char *ascii_key)
{
    int		i = 0, ak_idx;
    int		a, b;

    ak_idx = 0;
    for(i = 0; i < 8; i++) {
    	a = (int)((binary_key[i] >> 4) & 15);
	b = (int)(binary_key[i] & 15);

	if((a < 0) || (a > 15) || (b < 0) || (b > 15)) {
	    fprintf(stderr, "Math is hard, let's go shopping.\n");
	    return -1;
	}

	ascii_key[ak_idx++] = ((a < 10) ? ('0'+a) : ('a'+(a-10)));
	ascii_key[ak_idx++] = ((b < 10) ? ('0'+b) : ('a'+(b-10)));

	if((i % 2) && (i != 7)) {
	    ascii_key[ak_idx++] = '-';
	}
    }

    return 0;
}

static int 
mv_generate_key(unsigned char *out_key)
{
    MD5_CTX	ctx = { 0 };
    unsigned char	random_bytes[4];
    unsigned char	md5_buffer[16];
    int		i = 0;

    if(RAND_bytes(random_bytes, 8) != 1) {
    	fprintf(stderr, "Error generating random bytes.\n");
    	return -1;
    }

    MD5_Init(&ctx);
    MD5_Update(&ctx, random_bytes, 4);
    MD5_Update(&ctx, MV_DEMO_KEY, strlen(MV_DEMO_KEY));
    MD5_Final(md5_buffer, &ctx);

    // fold the first time
    for(i = 0; i < 8; i++) {
    	md5_buffer[i] ^= md5_buffer[i+8];
    }

    // fold the second time
    for(i = 0; i < 4; i++) {
    	md5_buffer[i] ^= md5_buffer[i+4];
    }

    memset(out_key, 0, 8);
    memcpy(out_key, random_bytes, 4);
    memcpy(&(out_key[4]), md5_buffer, 4);

    memset(md5_buffer, 0, 16);
    memset(random_bytes, 0, 4);

    return 0;
}

static int
mv_do_gen_bulk_keys(int number_of_keys)
{
    int			i, j;
    unsigned char	**key_buffer = NULL;
    unsigned char	ascii_key[20];
    int			err = 0;

    if(number_of_keys <= 0) {
        fprintf(stderr, "Requested number of keys is negative or 0.\n");
    	return -1;
    }

    // allocate the space for keys
    key_buffer = (unsigned char **)calloc(number_of_keys, sizeof(char *));
    if(key_buffer == NULL) {
    	fprintf(stderr, "Memory allocation error.\n");
	return -1;
    }
    for(i = 0; i < number_of_keys; i++) {
    	key_buffer[i] = (unsigned char *)calloc(8, sizeof(char));
	if(key_buffer[i] == NULL) {
	    err = 1;
	    break;
	}
    }
    if(i != number_of_keys) {
    	for(j = 0; j < i; j++) {
	    if(key_buffer[j]) {
	    	free(key_buffer[j]);
	    }
	}
	free(key_buffer);
	return -1;
    }

    j = 0;
    i = 0;
    while(i < number_of_keys) {
        // generte a key
    	if(mv_generate_key(key_buffer[i]) != 0) {
    	    err = 1;
	    break;
	}

	// check for duplicates
	for(j = 0; j < i; j++) {
	    if(memcmp(key_buffer[i], key_buffer[j], 8) == 0) {
	    	memset(key_buffer[i], 0, 8);
		continue;
	    }
	}

	i++;
    }

    if(i == number_of_keys) {
    	for(j = 0; j < number_of_keys; j++) {
	    memset(ascii_key, 0, 20);
    	    if(mv_get_printable_key(key_buffer[j], ascii_key) != 0) {
	    	err = 1;
	    	break;
	    }
	    fprintf(stdout, "%s\n", ascii_key);
	}
    }

    for(j = 0; j < i; j++) {
    	if(key_buffer[j]) {
	    free(key_buffer[j]);
	}
    }
    free(key_buffer);

    if(err) {
    	return -1;
    }

    return 0;
}

static int
mv_do_gen_key(void)
{
    unsigned char 	binary_key[8];
    unsigned char	ascii_key[20];

    memset(binary_key, 0, 8);
    memset(ascii_key, 0, 20);

    if(mv_generate_key(binary_key) != 0) {
    	return -1;
    }

    if(mv_get_printable_key(binary_key, ascii_key) != 0) {
    	return -1;
    }

    ascii_key[19] = 0;
    fprintf(stdout, "%s\n", ascii_key);
    return 0;
}
#endif /* !MV_VERIFY_KEY_ONLY */

void 
usage(char *cmd)
{
    fprintf(stderr, "usage is one of the following:\n");
#ifndef MV_VERIFY_KEY_ONLY
    fprintf(stderr, "    %s gen_key\n", cmd);
    fprintf(stderr, "    %s gen_bulk_keys <n keys>\n", cmd);
#endif /* !MV_VERIFY_KEY_ONLY */
    fprintf(stderr, "    %s verify_key <key>\n", cmd);
}

#define COMMAND_GEN_KEY			1
#define COMMAND_GEN_BULK_KEYS		2
#define COMMAND_VERIFY_KEY		3

int 
main(int argc, char **argv)
{
    int	command = 0;
    char *cmd = argv[0];
    unsigned char ascii_key_to_verify[20];
    int rval = 0;
#ifndef MV_VERIFY_KEY_ONLY
    int number_of_keys = 0;
    char *dummy = NULL;
    
    if((argc < 2) || (argc > 3)) {
    	usage(cmd);
	exit(-1);
    }

    if((strlen(argv[1]) == strlen("verify_key")) &&
    		(strcasecmp(argv[1], "verify_key") == 0)) {
	if(argc != 3) {
	    usage(cmd);
	    exit(-1);
	}
	if(strlen(argv[2]) != 19) {
	    fprintf(stderr, "key specified is not the correct length.\n");
	    usage(cmd);
	    exit(-1);
	}
	memset(ascii_key_to_verify, 0, 20);
	memcpy(ascii_key_to_verify, argv[2], 19);
	command = COMMAND_VERIFY_KEY;
    } else if((strlen(argv[1]) == strlen("gen_key")) &&
    		(strcasecmp(argv[1], "gen_key") == 0)) {
	if(argc != 2) {
	    usage(cmd);
	    exit(-1);
	}
	command = COMMAND_GEN_KEY;
    } else if((strlen(argv[1]) == strlen("gen_bulk_keys")) &&
    		(strcasecmp(argv[1], "gen_bulk_keys") == 0)) {
	if(argc != 3) {
	    usage(cmd);
	    exit(-1);
	}
	command = COMMAND_GEN_BULK_KEYS;
	number_of_keys = (int)strtol(argv[2], &dummy, 10);
	if(!dummy || (*dummy != '\0')) {
	    fprintf(stderr, "number of keys incorrect.\n");
	    usage(cmd);
	    exit(-1);
	}
    }

    switch(command) {
    	case COMMAND_GEN_KEY:
	    rval = mv_do_gen_key();
	    break;
	case COMMAND_GEN_BULK_KEYS:
	    rval = mv_do_gen_bulk_keys(number_of_keys);
	    break;
	case COMMAND_VERIFY_KEY:
	    rval = mv_do_verify_key(ascii_key_to_verify);
	    break;
	default:
	    fprintf(stderr, "Unknown command.\n");
	    rval = -1;
    }

    return rval;

#else /* MV_VERIFY_KEY_ONLY */
    if (argc != 2) {
        fprintf(stderr, "    %s <key>\n", cmd);
	exit(-1);
    }
    if(strlen(argv[1]) != 19) {
        fprintf(stderr, "key specified is not the correct length.\n");
        exit(-1);
    }
    memset(ascii_key_to_verify, 0, 20);
    memcpy(ascii_key_to_verify, argv[1], 19);
    return mv_do_verify_key(ascii_key_to_verify);
#endif /* MV_VERIFY_KEY_ONLY */
}
