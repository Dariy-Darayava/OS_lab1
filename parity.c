#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#include "plugin_api.h"

//#define fsize statbuf.st_size
int plugin_get_info(struct plugin_info* ppi)
{
	if (ppi == NULL)
		return 1;
	ppi->plugin_name = "parity";
	ppi->sup_opts_len = 1;
	
	struct plugin_option *sup_opts = calloc(1, sizeof(struct plugin_option));
	sup_opts->opt.name = "parity";
	sup_opts->opt.has_arg = required_argument;
	sup_opts->opt.flag = NULL;
	sup_opts->opt.val = 0;	

	sup_opts->opt_descr = "This plugin includes -parity key. When applied it will allow search for files with more odd  bytes(--parity odds), more even bytes(evens), or equal odd and even bytes(eq)";

	ppi->sup_opts = sup_opts;
	return 0;
}

int plugin_process_file(const char *fname,
        struct option *in_opts[],
        size_t in_opts_len,
        char *out_buff,
        size_t out_buff_len)
{
	int fd = open(fname, O_RDONLY);
	struct stat statbuf;


	unsigned long odd = 0;
	unsigned long even = 0;

	char mode;// 0 - even. 1 - odd. -1 - eq
	//printf("12name:%s\nha:%d\nflag:%s\n", in_opts[0]->name, in_opts[0]->has_arg, in_opts[0]->flag);

	char *arg = (char *)in_opts[0]->flag;
	if (arg == NULL)
	{
		sprintf(out_buff, "No arg for parity plugin\n");
		return -12;
	}
	if ((strncmp(arg, "evens", 5) == 0) && (strlen(arg) == strlen("evens")))
		mode = 0;
	else if ((strncmp(arg, "odds", 4) == 0) && (strlen(arg) == strlen("odds")))
		mode = 1;
	else if ((strncmp(arg, "eq", 2) == 0) && (strlen(arg) == strlen("eq")))
		mode = -1;
	else
	{
		sprintf(out_buff, "Incorrect arg for parity plugin\n");
		return -13;

	}

	if (fd == -1)
	{
		snprintf(out_buff, out_buff_len, "parity plugin can't open given file\n");
		//close(fd);
		return -1;
	}
	//get stat for file size
	if (fstat(fd, &statbuf) != 0)
	{
		
		snprintf(out_buff, out_buff_len, "parity plugin access file's stats\n");
		//close(fd);
		return -2;
	}

	off_t fsize =  statbuf.st_size;
	/*if (fsize == 0)
	{
		snprintf(out_buff, out_buff_len, "File has size 0\n");
		return -11;
	}*/
	unsigned int psize = getpagesize();
	//psize = 3;
	//printf("%d-%d|%s size:%di\n", mode, fd, fname, fsize);
	//mapping 
	char *p;
	for (int i = 0; i < fsize / psize; i++)
	{
		p = mmap(NULL, psize, PROT_READ, MAP_PRIVATE, fd, i*psize);
		if (p == MAP_FAILED)
		{
			snprintf(out_buff, out_buff_len, "mmap-1 failed at i:%d\n", i);
			return -4;
		}
		for(int j = 0; j < psize; j++)
		{
			//printf("%X ", p[j]);
			if ((p[j] & 1) == 0)
				even++;
			else
				odd++;
		}
		if (munmap(p, psize) != 0)
		{
			snprintf(out_buff, out_buff_len, "munmap-1 failed at i:%d\n", i);			
			return -3;
		}
		
			
	}
	//printf("\n--%d--\n", fsize / psize);
	if (fsize % psize != 0)
	{//check the end of the file
		 
		p = mmap(NULL, fsize % psize, PROT_READ, MAP_PRIVATE, fd, (fsize / psize)*psize);
		//p = mmap(NULL, 1, PROT_READ, MAP_PRIVATE, fd, 0);
		
		//printf("%X\n", (p[2] & 1));
		if (p == MAP_FAILED)
		{

			snprintf(out_buff, out_buff_len, "mmap-2 failed\n");
			return -5;
		}
		for (int i = 0; i < fsize % psize; i++)
		{
			//printf("%X ", p[i]);
			if((p[i] & 1) == 0)
				even++;
			else
				odd++;
				
		}
		if (munmap(p, psize) != 0)
		{

			snprintf(out_buff, out_buff_len, "munmap-2 failed\n");
			return -6;
		}
		
	}
	//printf("\nodd:%d\neven:%d\n", odd, even);
	switch(mode)
	{
		case 0:
			if (even > odd)
				return 0;
			else
				return 1;
			break;
		case 1:
			if (odd > even)
				return 0;
			else
				return 1;
			break;
		case -1:
			if (even == odd)
				return 0;
			else
				return 1;
			break;
		default:
			return -10; 
	}	
	close(fd);
	return 0;	
}

