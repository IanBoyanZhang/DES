#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <getopt.h> 

#include "DES.h"

//////////////////////////////////////////////////////
//                 GLOBAL VARIABLES                //
////////////////////////////////////////////////////

static FILE * output = NULL;

//////////////////////////////////////////////////////
//                 FUNCTIONS                       //
////////////////////////////////////////////////////

// usage 
static void usage(int status)
{
    if(status == EXIT_SUCCESS)
    {
	fprintf(stdout,"Usage: desbox [OPTION] -k=KEY FILE\n"
		"Encrypt or Descrypt with DES.\n\n"
		" -k, --key=KEY     required 64bits key\n"
		" -d, --decrypt     decrypt DES from input file\n"
		" -e, --encrypt     encrypt DES from input file\n"
		" -o, --output=FILE write result to FILE\n"
		" -h, --help        display this help\n");
    }
    else
    {
	fprintf(stderr, "Try 'desbox --help' for more information.\n");
    }
    exit(status);
}
// a supprimer
void printbits(uint64_t v)
{
    for(int ii = 0; ii < 64; ii++)
    {
	if( ((v << ii) & FIRSTBIT) == (uint64_t)0)
	    printf("0");
	else
	    printf("1");
    }
}

int main(int argc, char ** argv)
{
    // vars
    uint64_t key = 0;
    bool encrypt = true;
    FILE * input = NULL;

    //////////////////////////////////////////////////////
    //                 OPTION PARSER                   //
    ////////////////////////////////////////////////////

    int optc = 0;

    const char* short_opts = "dehk:o:";

    const struct option long_opts[] = 
	{ 
	    {"decrypt",        no_argument, NULL, 'd'},
	    {"encrypt",        no_argument, NULL, 'e'},
	    {"help",           no_argument, NULL, 'h'},
	    {"output",   required_argument, NULL, 'o'},
	    {"key",      required_argument, NULL, 'k'},
	    {NULL,                       0, NULL,   0}
	}; 

    while((optc = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1)
    { 
	switch(optc)
	{ 
	case 'd': // decrypt mode
	    encrypt = false;
	    break;

	case 'h': // help
	    usage(EXIT_SUCCESS);
	    break;

	case 'o': // output file
	    output = fopen(optarg, "w");
	    if(output == NULL)
	    {
		fprintf(stderr,
			"Error: don't have permission to write output file");
		exit(EXIT_FAILURE);
	    }
	    break;

	case 'k': // key
	    if(optarg)
	    {
		// 1. Convert: (char)key -> (uint64_t)key
		for(int ii = 0; optarg[ii] != '\0'; ii++)
		{
		    if(ii > 63)
		    {
			printf("Error: key is longer than 64bits \n");
			exit(EXIT_FAILURE);
		    }
		    if(optarg[ii] == '1')
			key += (FIRSTBIT >> ii);
		}
	    }
	    else
	    {
		usage(EXIT_FAILURE);
	    }
	    break;

	default : // no arguments
	    usage(EXIT_FAILURE); 
	}
    }

    //////////////////////////////////////////////////////
    //                CHECK ARGUMENTS                  //
    ////////////////////////////////////////////////////

    // Check if key has been given as input
    if(key == 0)
    {
	fprintf(stderr, "Error: You are supposed to pass a key as argument\n");
	usage(EXIT_FAILURE);
    }

    // Check if there is a input file passed as argument
    if(argv[optind] == NULL)
    {
	fprintf(stderr, "Error: Missing input file argument\n");
	usage(EXIT_FAILURE);
    }

    // Check if we can open the input file
    input = fopen(argv[optind], "rb");

    if(input == NULL)
    {
	fprintf(stderr, "Error: can't open input file\n");
	usage(EXIT_FAILURE);
    }

    // default output file if none is specified
    if(output == NULL) 
	output = fopen("output.txt", "w");

    // check if we have write rights
    if(output == NULL)
    {
	fprintf(stderr, "Error: don't have permission to write output file\n");
	exit(EXIT_FAILURE);
    }

    //////////////////////////////////////////////////////
    //                      APP                        //
    ////////////////////////////////////////////////////

    //
    // 1. Verify parity bits of the key
    //
    
    if(!key_parity_verify(key))
    {
	printf("The key you used is malformated\n"); // more error msg in function
	exit(EXIT_FAILURE);
    }
    
    //
    // 2. Get the 16 subkeys
    //

    uint64_t a_key[16];
    a_key[0] = key;
    uint64_t next_key;

    for(int ii = 0; ii < 16; ii++)
    {
        key_schedule(&a_key[ii], &next_key, ii);
        if(ii != 15)
            a_key[ii + 1] = next_key;
    }

    //
    // 3. 16 Rounds of enc/decryption
    //

    size_t amount; // used for fwrite
    uint64_t data;

    while((amount = fread(&data, 1, 8, input)) > 0)
    {
	if(amount != 8)
	{
	    data = data << (8 * (8 - amount));
	}
	printf("\n amount = %zd, data = ", amount);
	printbits(data);
	
	// initial permutation
	    Permutation(&data, true);

	// encrypt rounds
        if(encrypt)
        {
            for(int ii = 0; ii < 16; ii++)
            {
                rounds(&data, a_key[ii]);
            }
        }
	// decrypt rounds
        else
        {
	    data = (data << 32) + (data >> 32);

            for(int ii = 15; ii >= 0; ii--)
            {
                rounds(&data, a_key[ii]);
            }

	    data = (data << 32) + (data >> 32);
        }

	// final permutation
	    Permutation(&data, false);

	    printf("\n\nbeforewriting\n");

	if(amount != 8)
	{
	    data = data << (8 * (8 - amount));
	}
	printf("\n amount = %zd, data = ", amount);
	printbits(data);

	// write output
	fwrite(&data, 1, amount, output);
	data = 0;
    }

    fclose(input);
    fclose(output);

    //
    return EXIT_SUCCESS;
}
