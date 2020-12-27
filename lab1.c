#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>

#include "plugin_api.h"


#define MAX_STR_LEN 512
#define AND 1
#define OR 0

char version[] = "1.3.1";

static int N_key;
static int C_key;

static unsigned long found_count;

typedef struct option_info option_info;
typedef struct plugin plugin;

struct option_info
{
	char *long_name;
	char short_name;
	int has_arg;// just like struct option
	char *arg; 
	char was_set; // did we set this option or not 
	char *info;
	int affinity;// number of corresponding plugin in plugin_list (or -1 for default options)
	
};

static unsigned int option_count;
static option_info **option_list;


struct plugin
{
	char *path;//full name
	void *dlopen_pt;// dlopen_pt = dlopen(file)
	int (*get_info)(struct plugin_info *ppi);
	int (*process_file)(const char *fname, struct option *in_opts[], size_t in_opts_len, char *out_buff, size_t out_buff_len);
	
	struct plugin_info info;
	int opts_i;// number of the first option in option_list that belongs to this plugin
	int was_set;// 0 if none of the plugin optipns were accessed
	
};


static unsigned int plugin_count;
static plugin **plugin_list;
static unsigned int plugin_set_count;
static const int standart_opt_count = 7;

static int debug_mode = 0;


void _mexit(int code)
{
    printf("Something went wrong\n");
    exit(code);
}





void mnh(void *ptr, int exit_code)
{//because my fingers hurt writing if... exit...
	if(ptr == NULL)
	{
		//fprintf(2,"null malloc pointeri\n");
		_mexit(exit_code);
	}
}

void *mallscpy(char *src)
{//again saves time. Don't forget to free()
	char *ptr;
	if (src == NULL)
		//return NULL;
		_mexit(2200);
	if (strlen(src) >= MAX_STR_LEN)
		//return NULL;
		_mexit(3000);
	ptr = calloc(strlen(src) + 1,sizeof(char));
        mnh(ptr, 4000);
    strncpy(ptr, src,strlen(src));
        ptr[strlen(src)] = '\0';
	return ptr;
}


void add_option_to_list(char *long_name, char short_name, int has_arg, char* arg, char* info, int affinity);
void clear_option_list();

int attach_plugin(char *name);
int clear_plugin_list();



int search_dir(char *, unsigned int);

int main(int argc, char* argv[])
{
	
	//SETUP
	found_count = 0;

	plugin_count = 0;
	plugin_list = NULL;
	plugin_set_count = 0;
	
	option_count = 0;	
    option_list = NULL;

	opterr = 0;//disable standard error messages

	//add default options
	
	add_option_to_list("plugin", 'P', required_argument, NULL, "set path to plaguins. Arg required", -1);
	add_option_to_list("log", 'l', required_argument, NULL, "set path to the log. Arg required", -1);
	add_option_to_list("compare", 'C', required_argument, "and", "comparison type between plaguins. Arg required('and' or 'or'). Default value = 'and'", -1);
	add_option_to_list("inverse", 'N', no_argument, NULL, "inverse search condition(s). No arg", -1);
	add_option_to_list("version", 'v', no_argument, NULL, "show version. No arg", -1);
	add_option_to_list("help", 'h', no_argument, NULL, "help. No arg", -1);        
    add_option_to_list("debug", 'd', no_argument, NULL, "Print debug info. No arg", -1);

	

	
	//first run to find -P and -L and -d. Without hooking up plugins we can't analise argv correctly so we need two runs.

	while (1)
	{
		int c = -1;
		int option_index = 0;
		static struct option long_options_fs[] = {
                   {"plugin", required_argument, 0,  'P' },
                   {"log", required_argument, 0,  'l' },
                   {"compare", required_argument, 0,  'C' },
                   {"inverse", no_argument, 0,  'N' },
                   {"version", no_argument, 0,  'v' },
                   {"help", no_argument, 0,  'h' },
                   {"debug", no_argument, 0,  'd' },
                   {0,         0,                 0,  0 }
               };
		//c = getopt_long(argc, argv, "P:l:C:Nvh", long_options, &option_index);
		c = getopt_long(argc, argv, "-:P:l:C:Nvhd", long_options_fs, &option_index);
		if (c == -1)
			break;
		switch (c)
		{
			/*case 0:
			printf("option %s", long_options[option_index].name);
			if (optarg)
				printf(" with arg %s", optarg);
			printf("\n");
			break;*/

			case 1:
				//fprintf(stderr, "unknown arg on the first run\n");
				break;
			case 'P':
				if (option_list[0]->was_set != 0)
				{
					printf("There is several -P(--plugin). Exiting\n");
					_mexit(1300);
				}
				//printf("Found P\n");
				if (optarg)
				{
                                	//printf(" with arg %s", optarg);
					option_list[0]->arg = mallscpy(optarg);
					option_list[0]->was_set = 1;
				}
				else
				{
					printf("No arg for -P(--plugin). Exiting\n");
					_mexit(1200);
				}
				
   
				break;
			case 'l':
				if (option_list[1]->was_set !=0)
				{
					printf("There is several -l(--log) keys. Exiting\n");
					_mexit(1400);
				}
				if (optarg)
				{
					option_list[1]->arg = mallscpy(optarg);
					option_list[1]->was_set = 1;
				}
				else
				{
					printf("No arg for -l(--log). Exiting\n");
					_mexit(1500);
				}
				break;
			case 'C':
				break;
			case 'N':
				break;
			case 'v':
				break;
			case 'h':
				break;
            case 'd':
                if (option_list[6]->was_set !=0)
				{
					printf("There is several -d(--debug) keys\n");
					_mexit(1400);
				}
				option_list[6]->was_set++;
				break;
			case '?':
				//fprintf(s"Found unknown key\n");
				break;
			case ':':
				printf("Missing argument\n");
				_mexit(100000);
				break;
			default:
				printf("Unexpected result\n");
				break;
		}
	}
	if (option_list[6]->was_set != 0)
        debug_mode = 1;
    
	if (debug_mode) fprintf(stderr, "Analyze first scan results\n");
    if (debug_mode) fprintf(stderr, "First optarg scan was successful\n\n");
	//hooking up log file
	FILE *logf;
	if (option_list[1]->was_set == 1)
	{
        if (debug_mode) fprintf(stderr, "log file found:%s", option_list[1]->arg);
        
		option_list[1]->was_set = 0;
		if (option_list[1]->arg == NULL)
		{
			if (debug_mode) fprintf(stderr, "No arg for -l(--log)\n");
			_mexit(1501);
		}
		else
		{	
			logf = freopen(option_list[1]->arg, "w+", stderr);
			if (logf == NULL)
			{
				if (debug_mode) fprintf(stderr, "Error while opening log file\n");
				_mexit(1502);
			}

		}
	}
	
	//set -P
	if (debug_mode) fprintf(stderr,"Plugin dir:%s\n\n", option_list[0]->arg);
	option_list[0]->was_set = 0;
	
    


	//print argv for debugging
    if (debug_mode)
    {
        fprintf(stderr, "Original argv:\n");
        for (int i = 0; i < argc; i++)
            fprintf(stderr,"%s ", argv[i]);
        fprintf(stderr,"\n\n");
    }

    if (debug_mode) fprintf(stderr,"Checking getcwd()...\n");
	//get current working directory
	char *cwd = getcwd(NULL, MAX_STR_LEN);
        if(cwd == NULL)
        {
                if (debug_mode) fprintf(stderr, "Error in getcwd()\n");
                _mexit(1);
        }
        else
        {
                if (debug_mode) fprintf(stderr, "cwd:%s\n\n", cwd);
        }


	//WORK
	
	//pointers for plugin functions
	int (*get_info)(struct plugin_info *ppi);
        int (*process_file)(const char *fname, struct option *in_opts[], size_t in_opts_len, char *out_buff, size_t out_buff_len);

	DIR *dir;

	struct dirent *sd;

	char so_name[MAX_STR_LEN];

	//looking through current dir for plugins
    
    if (debug_mode) fprintf(stderr,"Scanning current working directory for .so files\n");
	dir = opendir(cwd);
	if (dir == NULL)
	{
		if (debug_mode) fprintf(stderr, "Can't open src dir(opendir() error)\n");
		_mexit(80085);
	}
	while (1)
	{
		sd = readdir(dir);
		if (sd == NULL)
		{//can be an error or the end of the dir
			if (debug_mode) fprintf(stderr, "NULL encountered while searching for plugins in src dir\n");
			if (closedir(dir) != 0)
   			{
                		if (debug_mode) fprintf(stderr, "Error in closedir() while closing cwd\n");
                		_mexit(413314);
            }

			break;
		}
		if (debug_mode) fprintf(stderr, "Assessing %s\n", sd->d_name);
		unsigned int len = strlen(sd->d_name);
		//printf("%s %d\n", sd->d_name, sd->d_type);
		if (sd->d_type == DT_REG)
		{//if it is a file
			//if it is .so file
			if(len > 3 && sd->d_name[len - 1] == 'o' && sd->d_name[len - 2] == 's' && sd->d_name[len - 3] == '.')
			//printf("%s\n", sd->d_name);
                if (realpath(sd->d_name, so_name) != NULL)
                {//get full path
                    //printf("%s\n",so_name);
                    int err = 0;
                    err = attach_plugin(so_name);
                    if ( err != 0)
                    {
                        if (debug_mode) fprintf(stderr, "Can't attach %s(Error:%d)\n", so_name,err);
                        continue;
                    }
                    else
                    {
                        if (debug_mode) fprintf(stderr, "%s attached\n", so_name);
                    }
                }
                else
                {
                    if (debug_mode) fprintf(stderr, "Can't resolve realpath for %s Skipping\n", sd->d_name);
                    continue;
                }
		}
		 
		
	}
	
	 
	//look through -P dir for plugins	
	if (option_list[0]->arg != NULL)
	{
        if (debug_mode) fprintf(stderr,"\nScanning -P directory for .so files\n");
		//change dir to search in it
        if(chdir(option_list[0]->arg) == 0)
        //if(chdir("../klab1/labaratornaya1/") == 0)
		{// -P is valid
		// test for -P; it should not point to cwd
            char *pd = getcwd(NULL, MAX_STR_LEN);
	        if(pd == NULL)
       		{
	               	if (debug_mode) printf("Error in getcwd() while comparing -P and cwd\n");
	                _mexit(45);
	        }
	        
            if (strcmp(cwd, pd) == 0)
            {
                if (debug_mode) fprintf(stderr, "Fatal error: -P points to cwd\n");
                _mexit(46);
            }
            free(pd);
            
            dir = opendir(".");
		        if (dir == NULL)
		        {
		                if (debug_mode) fprintf(stderr, "Can't open -P dir(opendir() error)\n");
		                _mexit(43);
		        }
		        while (1)
		        {
		                sd = readdir(dir);
		                if (sd == NULL)
		                {
		                        if (debug_mode) fprintf(stderr, "NULL encountered while searching for plugins in -P dir\n");
		                        if (closedir(dir) != 0)
		                        {
		                                if (debug_mode) fprintf(stderr, "Error in closedir() in -P\n");
                		                _mexit(44);
		                        }

                		        break;
		                }
		                if (debug_mode) fprintf(stderr, "Assessing %s\n", sd->d_name);
              			unsigned int len = strlen(sd->d_name);
		                if (sd->d_type == DT_REG)
		                {
		                        if(len > 3 && sd->d_name[len - 1] == 'o' && sd->d_name[len - 2] == 's' && sd->d_name[len - 3] == '.')
		                        if (realpath(sd->d_name, so_name) != NULL)
                		        {
		                                //printf("%s\n",so_name);
                                        int err = 0;
	        	                        err = attach_plugin(so_name);
		                                if ( err != 0)
	                	                {
	                        	                if (debug_mode) fprintf(stderr, "Can't attach %s(%d)\n", so_name,err);
	                                	        continue;
                                        }
                                        else
                                        {
                                            if (debug_mode) fprintf(stderr, "%s attached\n", so_name);
                                        }

		                        }
		                        else
		                        {
		                                if (debug_mode) fprintf(stderr, "Can't resolve realpath for %s Skipping\n", sd->d_name);
		                                continue;
		                        }
		                }


		        }



			if(chdir(cwd) != 0)
			{	
                if (debug_mode) fprintf(stderr, "Fatal error: cannot change -P dir back to cwd\n");
                _mexit(40);
			}
		}
		else
		{// -p is not valid
			if (debug_mode) fprintf(stderr, "Fatal error: path written in -P[%s] is not valid(chdir() errno:%d)\n",option_list[0]->arg,  errno);
            if (debug_mode) perror(NULL);
			_mexit(41);
		}
	}
	
	



	//PARCING
	//construct log_option for getopt_long()
	struct option *getopt_long_options = malloc((option_count + 1)*sizeof(struct option));
	mnh(getopt_long_options, 55);	
	for(int i = 0; i < option_count; i++)
	{
		getopt_long_options[i].name = option_list[i]->long_name;
		getopt_long_options[i].has_arg = option_list[i]->has_arg;
		getopt_long_options[i].flag = NULL;
		getopt_long_options[i].val = 0;
		

	}
	getopt_long_options[option_count].name = 0;	
	getopt_long_options[option_count].has_arg = 0;	
	getopt_long_options[option_count].flag = 0;	
	getopt_long_options[option_count].val = 0;	
	
	optind = 0;// reset the scan
	//scan
	while(1)
	{
		int c = -1;
		int option_index = 1;

		c = getopt_long(argc, argv, "+:P:l:C:Nvhd", getopt_long_options, &option_index);
		//c = getopt_long(argc, argv, "+:P:", getopt_long_options, &option_index);
		
		if (c == -1)
			break;

		switch(c)
		{
			case 0://long option encountered
				if (option_list[option_index]->was_set != 0)
				{
					if (debug_mode) fprintf(stderr, "--%s was set twice or more. Exiting..", option_list[option_index]->long_name);
					_mexit(61);
				}
				option_list[option_index]->was_set++;
				plugin_list[option_list[option_index]->affinity]->was_set++;
				if ((option_list[option_index]->has_arg != no_argument) && (optarg != NULL) )
					option_list[option_index]->arg = mallscpy(optarg);
				break;
			case 'P':
				if (option_list[0]->was_set != 0)
				{
					
					if (debug_mode) fprintf(stderr, "-P(--plugin) was set twice or more. Exiting...\n");
					_mexit(62);
				}
				option_list[0]->was_set++;
				//if (option_list[0]->has_arg != no_argument)

				// no need to set -P. it was set at the start;
				//option_list[0]->arg
				break;
			case 'l':
				if (option_list[1]->was_set != 0)
                                {

                                        if (debug_mode)fprintf(stderr, "-l(--log) was set twice or more. Exiting...\n");
                                        _mexit(63);
                                }
                                option_list[1]->was_set++;
				//option_list[1]->arg = mallscpy(optarg);
				break;
			case 'C':
				if (option_list[2]->was_set != 0)
                                {

                                        if (debug_mode)fprintf(stderr, "-C(--compare) was set twice or more. Exiting...\n");
                                        _mexit(64);
                                }
                                option_list[2]->was_set++;
                                option_list[2]->arg = mallscpy(optarg);
				break;
			case 'N':
				if (option_list[3]->was_set != 0)
                                {

                                        if (debug_mode) fprintf(stderr, "-N(--inverse) was set twice or more. Exiting...\n");
                                        _mexit(65);
                                }
                                option_list[3]->was_set++;
				break;
			case 'v':
				if (option_list[4]->was_set != 0)
                                {

                                        if (debug_mode) fprintf(stderr, "-v(--version) was set twice or more. Exiting...\n");
                                        _mexit(66);
                                } 
                                option_list[4]->was_set++;

				break;
			case 'h':
				if (option_list[5]->was_set != 0)
                                {

                                        if (debug_mode) fprintf(stderr, "-h(--help) was set twice or more. Exiting...\n");
                                        _mexit(67);
                                } 
                                option_list[5]->was_set++;

				break;
            case 'd':
                /*if (option_list[6]->was_set == 0)
                {
                    fprintf(stderr, "-d(--debug) was found on the second scan but not on the first. Exiting\n");
                    _mexit(6969);
                }
				if (option_list[6]->was_set == 1)
                    option_list[6]->was_set++;
                if (option_list[6]->was_set == 2)
                                {

                                        fprintf(stderr, "-h(--help) was set twice or more. Exiting...\n");
                                        _mexit(67);
                                } 
                */
				break;    
			case ':':
				if (debug_mode) fprintf(stderr, "Missing argument for one of the keys. Exiting\n");
				_mexit(60);
				break;
			case '?':
				if (debug_mode) fprintf(stderr, "Found unknown key. Exiting...\n");
				_mexit(61);
				break;
			default:
				if (debug_mode) fprintf(stderr, "Unexpected error during [%s]option parcing %d(%c)[%d]\n",argv[optind - 1], c,c,optind -1);
				_mexit(62);
		}

		
	}
	//count number of plugins set
	for (int i = 0; i < plugin_count; i++)
	{
		if (plugin_list[i]->was_set != 0)
			plugin_set_count++;
	}
	
	
	//Log all recorded options
	if (debug_mode)
    {
        fprintf(stderr,"\nSecond optarg scan is successful.\nRecorded options:\n");
        for(int i = 0; i < option_count; i++)
            {
                    fprintf(stderr, "%s(%d):",option_list[i]->long_name, option_list[i]->was_set);
                    if((option_list[i]->was_set != 0) && (option_list[i]->has_arg != no_argument))
                            fprintf(stderr,"%s", option_list[i]->arg);
                    fprintf(stderr,"\n");
            }
        fprintf(stderr, "\n");
    }


	//show version if asked
    if (debug_mode) fprintf(stderr, "Checking -v...\n");
	if(option_list[4]->was_set)
	{
		printf("Version:%s\n",version);
		//fprintf(stderr, "Version:%s\n",version);
	}
	
    if (debug_mode) fprintf(stderr, "Checking -N...\n");
	if (option_list[3]->was_set != 0)// -N was set
    {
        N_key = 1;
    }
	else
	{
		N_key = 0;
	}
	
	if (debug_mode) fprintf(stderr, "Checking -C...\n");
    if (option_list[2]->was_set != 0)//-C was set. And = 0; Or = 1
    {
		C_key = 0;//Default and
        char *C_check = mallscpy(option_list[2]->arg);
        for (int i = 0; i < strlen(C_check); i++)
        {
            C_check[i] = tolower(C_check[i]);
        }
        if((strncmp(C_check, "and", 3) == 0) && (strlen(C_check) == strlen("and")))
            C_key = 0;
        else if((strncmp(C_check, "or", 2) == 0) && (strlen(C_check) == strlen("or")))
			C_key = 1;
        else
        {
            if (debug_mode) fprintf(stderr, "Wrong -C arg\n");
            _mexit(85);
        }
        free(C_check);
    }

    if (debug_mode) fprintf(stderr, "Checking -h...\n\n");
	//Help was called (-h or --help)
	if(option_list[5]->was_set)
        {
                printf("Help was called\n");
                printf("This programm performs recursive search and seeks files based on hooked up plugins(\".\" is defaut plugin search dir)\n");
		printf("If you see no output at all, error occured. Check log\n");
                printf("This programm acceps options in following format:\n");
                printf("./lab1 [Options] [Search dir]\n");
                printf("\nStandart options:\n");
                for (int i = 0; i < standart_opt_count; i++)
                {
                        printf("--%s(-%c): %s\n",option_list[i]->long_name, option_list[i]->short_name, option_list[i]->info);
                }
                printf("\nTo acces plugin option info run\n");
                printf("./lab [-P plugin dir]\n");
                return 0;
        }



	//printf("%d %d\n", C_key, N_key);
    
	//analyze rest of argv
	if (optind == argc)
	{//no search dir. print relevant info
		printf("No search dir detected. Showing info\n");
		
		printf("\nPlugins:\n");
		for(int i = 0; i < plugin_count; i++)
		{
			printf("%d:%s\n", i, plugin_list[i]->info.plugin_name);
			//printf("%s\n");
			for(int j = 0; j < plugin_list[i]->info.sup_opts_len; j++)
			{
				printf("Opt[%d]:--%s %s\n", j, plugin_list[i]->info.sup_opts[j].opt.name, plugin_list[i]->info.sup_opts[j].opt_descr);
			}
		}

		printf("\n\nStandart options:\n");
		for (int i = 0; i < standart_opt_count; i++)
                {
                        printf("--%s(-%c): %s\n",option_list[i]->long_name, option_list[i]->short_name, option_list[i]->info);
                }

		
	}
	else if (optind == argc - 1)
	{//search dir detected. Is it correct?
        if (debug_mode) fprintf(stderr, "There is one arg after options. Checking validity\n");
		if(chdir(argv[optind]) == 0)
		{//correct search dir. searching
			if (debug_mode) fprintf(stderr, "Correct search dir. Searching\n");
			//check plugins
			if (plugin_count == 0)
			{
				if (debug_mode) fprintf(stderr, "Error. No plugins detected while search dir is present. Please provide at least one plugin.\n");
				_mexit(101);
			}
			if (plugin_set_count == 0)
			{
				if (debug_mode) fprintf(stderr, "Error. No plugin options were detected while search dir is present. please add an option\n");
				_mexit(1233211);
			}
			
			//for (int i = 0; i < 0;)
			

			search_dir(".", 0);
			printf("Found:%ld files\n", found_count);	
			if(chdir(cwd) != 0)
			{
				if (debug_mode) fprintf(stderr, "Can't switch back to cwd after recursive search\n");
			}
			
		}
		else
		{//incorrect exiting
			if (debug_mode) fprintf(stderr, "Incorrect search dir(chdir() error). Exiting...\n");
			_mexit(70);
		}
		
	}
	else
	{
		if (debug_mode) fprintf(stderr,"More than one arg after options or there is an extra arg in the middle of key sequence(%d). Exiting...\n", optind );
		_mexit(70);
	}


	if (debug_mode) fprintf(stderr, "\nCleanup\nfree(cwd)\n");
	//CLEANUP
	free(cwd);
	if (debug_mode) fprintf(stderr, "free(getopt_long_options)\n");
	free(getopt_long_options);	
    
    if (debug_mode) fprintf(stderr, "Clear options\n");
	clear_option_list();
    if (debug_mode) fprintf(stderr, "Clear plugins\n");
	clear_plugin_list();	

	return 0;
}


//FUNCTIONS

void add_option_to_list(char *long_name, char short_name, int has_arg, char* arg, char *info, int affinity)
{
	//basically fill an array
	option_count++;
	if(option_count == 1)//we are about to add our first option
	{
		//initialize option list
		option_list = malloc(1*sizeof(option_info*));
		mnh((void *)option_list, 1);
		option_list[option_count - 1] = malloc(1*sizeof(option_info));
		mnh((void *)option_list[option_count - 1], 2);
	}
	else
	{
		option_list = realloc(option_list, option_count*sizeof(option_info*));
		mnh((void *)option_list, 3);
		option_list[option_count - 1] = malloc(1*sizeof(option_info));
		mnh((void *)option_list[option_count - 1], 4);
	}
	//fill new option_info
	if(long_name != NULL)
	{
		/*option_list[option_count - 1]->long_name = malloc(sizeof(char)*strlen(long_name + 1));
		mnh((void *)option_list[option_count - 1]->long_name , 5);
		strncpy(option_list[option_count - 1]->long_name, long_name,strlen(long_name) + 1);
		option_list[option_count - 1]->long_name[strlen(long_name)] = '\0';*/
        option_list[option_count - 1]->long_name = strndup(long_name, MAX_STR_LEN);
        mnh((void *)option_list[option_count - 1]->long_name , 5);
        
	}
	else
	{
		
		option_list[option_count - 1]->long_name = NULL;
	}

	/*if(short_name != NULL)
	{
		option_list[option_count - 1]->short_name = malloc(sizeof(char)*strlen(short_name + 1));
		mnh((void *)option_list[option_count - 1]->short_name , 6);
		strncpy(option_list[option_count - 1]->short_name, short_name,strlen(short_name));
		option_list[option_count - 1]->short_name[strlen(short_name)] = '\0';
	}
	else
		option_list[option_count - 1]->short_name = NULL;*/

	option_list[option_count - 1]->short_name = short_name;

	option_list[option_count - 1]->has_arg = has_arg;

	if(arg != NULL)
	{
		/*option_list[option_count - 1]->arg = malloc(sizeof(char)*strlen(arg + 1));
        mnh((void *)option_list[option_count - 1]->arg , 7);
        strncpy(option_list[option_count - 1]->arg, arg,strlen(arg));
        option_list[option_count - 1]->arg[strlen(arg)] = '\0';*/
        option_list[option_count - 1]->arg = strndup(arg, MAX_STR_LEN);
        mnh((void *)option_list[option_count - 1]->arg , 7);

	}
	else
		option_list[option_count - 1]->arg = NULL;

	option_list[option_count - 1]->was_set = 0;

	if(info != NULL)
	{
		/*option_list[option_count - 1]->info = malloc(sizeof(char)*strlen(info + 1));
        mnh((void *)option_list[option_count - 1]->info , 8);
        strncpy(option_list[option_count - 1]->info, info,strlen(info));
        option_list[option_count - 1]->info[strlen(info)] = '\0';*/
        
        option_list[option_count - 1]->info = strndup(info, MAX_STR_LEN);
        mnh((void *)option_list[option_count - 1]->info , 8);
	}
	else
		  option_list[option_count - 1]->info = NULL;

	option_list[option_count - 1]->affinity = affinity;

		
}




void clear_option_list()
{
	if (option_count == 0) return;
    
	for (int i = 0; i < option_count ; i++)
	{
        if (debug_mode) fprintf(stderr, "Clearing option: %s\n",option_list[i]->long_name);
		if(option_list[i]->long_name != NULL) free(option_list[i]->long_name);
		//if(option_list[i]->short_name != NULL) free(option_list[i]->short_name);
		//do nothing to has_arg
        
		if(option_list[i]->arg != NULL) free(option_list[i]->arg);
		//do nothing to was_set
            ;
		if(option_list[i]->info != NULL) free(option_list[i]->info);
		//do nothing to affinity
        
        //printf("%p\n", option_list[i]);
        //if(i != 3)
		free(option_list[i]);
	}
        
	free(option_list);
	option_list = NULL;
	option_count = 0;
}



int attach_plugin(char *name)
{
	void *pt = dlopen(name, RTLD_LAZY||RTLD_GLOBAL);
	if (!pt)
	{
		return -1;
	}
	int (*get_info)(struct plugin_info *ppi) = dlsym(pt, "plugin_get_info");
	if (get_info == NULL)
		return -2;
    int (*process_file)(const char *fname, struct option *in_opts[], size_t in_opts_len, char *out_buff, size_t out_buff_len) = dlsym(pt, "plugin_process_file");
	if (process_file == NULL)
		return -3;
	//If we are here, both functions are found. Lets save this plugin
	


	//check if we already have this plugin plugged in
	/*plugin_info curr_plugin_info;
	int ret = get_info(&curr_plugin_info);
	if (ret != 0)
		return ret;
	
	for (int i = 0; i < plugin_count; i++)
	{
		if (strncmp(curr_plugin_info.plugin_name, plugin_list[i]->info.plugin_name, MAX_STR_LEN) == 0)
			return 150;
	}*/
	plugin_count++;
	if (plugin_count == 1)
	{//first plugin
		plugin_list = malloc(sizeof(plugin *));
		mnh(plugin_list, 50);
		plugin_list[plugin_count - 1] = malloc(sizeof(plugin));
		mnh(plugin_list[plugin_count - 1], 51);
		
	}
	else
	{//firstn't plugin
		plugin_list = realloc(plugin_list, plugin_count*sizeof(plugin*));
		mnh(plugin_list, 52);
		plugin_list[plugin_count - 1] = malloc(sizeof(plugin));
		mnh(plugin_list[plugin_count - 1], 53);
	}	

	//filling info
	plugin_list[plugin_count - 1]->path = mallscpy(name);
	plugin_list[plugin_count - 1]->dlopen_pt = pt;
	plugin_list[plugin_count - 1]->get_info = get_info;
	plugin_list[plugin_count - 1]->process_file = process_file;


	//get plugin info from plugin function	
	int ret =  plugin_list[plugin_count - 1]->get_info(&plugin_list[plugin_count - 1]->info);
	if (ret != 0)
		return ret;
	
	//plugin_list[plugin_count - 1]-> info = 

	//options
	for (int i = 0; i < plugin_list[plugin_count - 1]->info.sup_opts_len; i++)
	{
		add_option_to_list( (char *)plugin_list[plugin_count - 1]->info.sup_opts[i].opt.name, -1, plugin_list[plugin_count -1]->info.sup_opts[i].opt.has_arg, NULL, (char *)plugin_list[plugin_count - 1]->info.sup_opts[i].opt_descr, plugin_count - 1);
		//printf("%d\n", plugin_list[plugin_count - 1]->info.sup_opts[0].opt.has_arg);
		
	}
	plugin_list[plugin_count - 1]->opts_i = option_count - plugin_list[plugin_count - 1]->info.sup_opts_len + 1 - 1;
	plugin_list[plugin_count - 1]->was_set = 0;
	return 0;
}

int clear_plugin_list()
{
	if (plugin_count == 0)
		return 0;
	for (int i = 0; i < plugin_count; i++)
	{
        if (debug_mode) fprintf(stderr, "Clearing plugin: %s\n",plugin_list[i]->path);
		free(plugin_list[i]->path);
        free(plugin_list[i]->info.sup_opts);
		dlclose(plugin_list[i]->dlopen_pt);
        
        free(plugin_list[i]);
	}
	free(plugin_list);
	
	plugin_list = NULL;
	plugin_count = 0;
}

int search_dir(char *dname, unsigned int depth)
{

	static int prep = 0;
	
	static char **out_buffs = NULL;
	static struct option ***in_opts_arr = NULL;// array of array of pointers to options
	if (prep == 0)//So we need to transform our big data structures to smaller ones for a plugin function
	{//create necessary data structures
		prep = 1;
		//printf("staaaaaaatic noise\n");
		out_buffs = malloc(plugin_count*sizeof(char *));
		mnh(out_buffs, 81);
		in_opts_arr = malloc(plugin_count*sizeof(struct option **));// array of array of pointers to options
		mnh(in_opts_arr, 80);
		for(int i = 0; i < plugin_count;i++)
		{//for  every plugin we create struct option *in_opts[] and out_buff;
			out_buffs[i] = (char *)calloc((MAX_STR_LEN + 1), sizeof(char));//out_buff is done
			mnh(out_buffs[i], 82);
			in_opts_arr[i] = (struct option **)malloc(plugin_list[i]->info.sup_opts_len*sizeof(struct option *));
			mnh(in_opts_arr[i], 83);
			for (int j = 0; j < plugin_list[i]->info.sup_opts_len; j++)
			{
				in_opts_arr[i][j] = malloc(sizeof(struct option));
				mnh(in_opts_arr[i][j], 84);
				in_opts_arr[i][j]->name = option_list[plugin_list[i]->opts_i + j]->long_name;
				in_opts_arr[i][j]->has_arg = option_list[plugin_list[i]->opts_i + j]->has_arg;
				if (in_opts_arr[i][j] != no_argument)
					in_opts_arr[i][j]->flag = (int *)option_list[plugin_list[i]->opts_i + j]->arg;
				in_opts_arr[i][j]->val =0;
				
			}
		}
		/*//standart keys
		if (option_list[3]->was_set != 0)// -N was set
		{
			N_key = 1;
		}
		if (option_list[2]->was_set != 0)//-C was set
		{
			char *C_check = mallscpy(option_list[2]->arg);
			for (int i = 0; i < strlen(C_check); i++)
			{
				C_check[i] = tolower(C_check[i]);
			}
			if((strncmp(C_check, "and", 3) == 0) && (strlen(C_check) == strlen("and")))
				C_key = 1;
			else
			{
				fprintf(stderr, "Wrong -C arg\n");
				_mexit(85);
			}
		}*/
		
			
		
	}
	//printf("keeeys %d, %d", N_key, C_key);
	/*printf("///////////\n");
	for(int i = 0; i < plugin_count; i++)
	{
		printf("plugin%d\n", i);
		for(int j = 0; j < plugin_list[i]->info.sup_opts_len; j++)
		{
			printf("name:%s|has_arg:%d|flag:%s\n",in_opts_arr[i][j]->name, in_opts_arr[i][j]->has_arg,in_opts_arr[i][j]->flag);
		}
	}
	printf("///////////\n");*/
	
	DIR *dir;
	struct dirent *entry;
	//open dir and search through it
	if (!(dir = opendir(dname)))
		return -1;
    if (debug_mode) fprintf(stderr, "   Searching %s dir \n", dname);
	while ( (entry = readdir(dir)) != NULL )//while not end of dir
	{
        if (debug_mode) fprintf(stderr, "  Analyzing %s\n", entry->d_name);
		if (entry->d_type == DT_DIR)// if it is a dir
		{//recursive entry
			if ( (strncmp(entry->d_name, ".", MAX_STR_LEN) == 0) || (strncmp(entry->d_name, "..", MAX_STR_LEN) == 0) )
				continue;
			//create new string
			char *new_dname = calloc(strlen(dname) + 1/*'/'*/ + strlen(entry->d_name) + 1/*'\0'*/, sizeof(char));
			mnh(new_dname, 80);
			//attach dir_name/file_name
			strncat(new_dname, dname, MAX_STR_LEN);
			strncat(new_dname, "/", MAX_STR_LEN);
			strncat(new_dname, entry->d_name, MAX_STR_LEN);
			search_dir(new_dname, depth + 1);

			//printf("%s\n", new_dname);
			free(new_dname);
		}
		else if (entry->d_type == DT_REG)//if it is a regular file
		{//process file
			char *full_fname = calloc((strlen(dname) + 1 + strlen(entry->d_name) + 1),sizeof(char));
			mnh(full_fname, 86);

			//attach dir_name/file_name
			strncat(full_fname, dname, MAX_STR_LEN);
			strncat(full_fname, "/", MAX_STR_LEN);
			strncat(full_fname, entry->d_name, MAX_STR_LEN);
			char *real_fname = realpath(full_fname, NULL);


			int council = 0;//council must decide this file's fate
			//if plugins returns 0 council++
			// based on -C and -N All plugins must agree to print file, one of them should, or none
			for(int i = 0; i < plugin_count; i++)
			{
				if (plugin_list[i]->was_set == 0)
					continue;

				int rez;
				rez = plugin_list[i]->process_file(real_fname, in_opts_arr[i], plugin_list[i]->info.sup_opts_len, out_buffs[i], MAX_STR_LEN);
				if (rez == 0)
				{//plugin[i] deems this file worthy
					council++;
				}
				else if (rez > 0)
				{//plugin[i] rejects this file.
					
				}
				else if (rez < 0)
				{//plugin[i] can't assess this file. Error message shal be printed
					fprintf(stderr, "Error(%d). Plugin(%s) can't assess %s.\n%s\n", rez, plugin_list[i]->info.plugin_name, real_fname, out_buffs[i]);
					_mexit(100);
				}
					
				//printf("%s\n", real_fname);
			}

			/*if (plugin_count == 0)
			{// there is no plugins to decide. Print it anyway
                                printf("%s\n", real_fname);
			}*/
			if (C_key == 0)//And
			{
				if (council != plugin_set_count)// some plugins have diceded that this file is not worthy
					council = 0;
			
			}
			else//Or
			{
				if(council <= 0)//None decided this file worthy
					council = 0;
			}

			//printf("\n\n%d %d\n", ((N_key == 1)? 0 : 1), council);
			if (N_key == 1)//N was set
				council = !council;
			if (council)
			{// it is decided
				if (option_list[1]->was_set != 0)//log was set so no need for double output
					printf("Found:%s\n", real_fname);
				fprintf(stderr,"Found:%s\n", real_fname);
				found_count++;
			}
			free(full_fname);
			free(real_fname);
		}
	}
	if ((prep != 0) && (depth == 0))
	{//cleanup
		for (int i =0; i < plugin_count; i++)
		{
			free(out_buffs[i]);
			for (int j = 0; j < plugin_list[i]->info.sup_opts_len; j++)
			{
				free(in_opts_arr[i][j]);
			}
			free(in_opts_arr[i]);
		}
		free(out_buffs);
		free(in_opts_arr);
		prep = 0;
	}
	closedir(dir);
	return 0;
}
