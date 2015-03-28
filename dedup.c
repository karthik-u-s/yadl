#include "dedup.h"
#include "main.h"
#define NAME_SIZE 100

/*
Function to dedup a file whose path is specified by the user.
Input:char* filename,int chunk_type,int hash_type,int block_size
Output:int
*/
int
dedup_file (char* filename,int chunk_type,int hash_type,int block_size)
{

	int ret			=	-1;
	int fd_input		=	-1;
	int fd_cat		=	-1;
	int fd_stub 		= 	-1;
	char* buffer 		=	NULL;
	int length		=	0;
	int h_length		=	0;
	int b_offset		=	0;
	int e_offset		=	0;
	int size  		=  	0;
	int size1		=	0;
	char* buff		= 	NULL;
	char* hash		=	NULL;
	char* ts1		=	NULL;
	char* ts2		=	NULL;
	char* dir		=	NULL;
	char* filename1		=	NULL;
	char temp_name[NAME_SIZE]=	"";
	struct stat st;
	ts1 = strdup(filename);
	ts2 = strdup(filename);
	dir = dirname(ts1);
	filename1 = basename(ts2);
	sprintf(dir,"%s/",dir);
	sprintf(temp_name,"%sDedup_%s",dir,filename1);
	ret=file_exist(filename);
	if (ret== 0)
	{
		printf("\nFile %s does not exist in directory",filename);
		goto out;
	}
	
	fd_input =open(filename,O_APPEND|O_CREAT|O_RDWR);
	if (fd_input< 1)
	{
		printf("\nCreation of catalog file failed with error [%s]\n",
		strerror(errno));
		goto out;
	}
	fd_cat =open("filecatalog.txt",O_APPEND|O_CREAT|O_RDWR);
    if (fd_cat< 1)
    {
        printf("\nCreation of catalog file failed with error [%s]\n",
		strerror(errno));
        goto out;
    }
	ret=comparepath(filename,fd_cat);
		if (ret== -1)
		{
			goto out;
		}
		if (ret== 0)
		{
			printf("\nfile is already deduped");
			goto out;
		}
	fd_stub =open(temp_name,O_APPEND|O_CREAT|O_RDWR);
	if (fd_stub< 1)
	{
		printf("\nCreation of stub file failed with error [%s]\n",
		strerror(errno));
		goto out;
	}
	
	fstat(fd_input, &st);
	size = st.st_size;

	while(1)
	{
		if (size<= block_size)
		{
			block_size=size;
		}
	b_offset=e_offset;

	ret=get_next_chunk(fd_input, chunk_type,block_size,
	&buffer, &length);
	if (ret<= 0)
		break;

	printf("\nbuffer %s\n",buffer);
	printf("\nbuffer length %d\n",length);
	e_offset+=length-1;
	size=size-length;
	buff=(char*)calloc(1,length);
	strcpy(buff,buffer);
	ret=get_hash(buffer,length, hash_type,&hash,&h_length);
	printf("\nhash %s\n",hash);
	printf("length of hash is %d",h_length);
	if (ret== -1)
        goto out;
	printf("\nboffset %d eoffset %d\n",b_offset,e_offset);
	ret=chunk_store(buff,hash,length,h_length,b_offset,e_offset,fd_stub);
	if (ret== -1)	
        goto out;
	e_offset++;
	if (size<= 0)
	{
		ret=0;
		break;
	}
	buffer=NULL;
	hash=NULL;

}
	ret=writecatalog(fd_cat,filename);
	if (ret== -1)
	{
		goto out;
	}

ret=0;

out: return ret;

}

/*
Function to get next chunk from file.
Input:int fd_input,int chunk_type,int block_size,char** buffer,
int *length
Output:int
*/
int 
get_next_chunk(int fd_input,int chunk_type,int block_size,char** buffer,
int *length)
{

	int ret		=	 -1;
	*buffer=(char *)malloc(block_size+1);
	ret=read(fd_input,*buffer,block_size);
	if (ret< 1)
		goto out;
	*length=strlen(*buffer);
out:
	return ret;

}


/*
Function to get hash from a specific algorithm.
Input:char *buffer,int length,int hash_type,char** hash,int *h_length
Output:int
*/
int 
get_hash(char *buffer,int length,int hash_type,char** hash,int *h_length)
{
	
	int ret		=	-1;
	char *buf	=	NULL;
	char *buff	=	NULL;

	switch(hash_type)
	{
		case 1:
			buf = str2md5(buffer, length);
			*hash = buf;
			*h_length=strlen(buf);;
			break;
		case 2:
			buff =sha1(buffer, length);
			*hash = buff;
			*h_length=strlen(buff);
			break;
	}
	ret=0;
	return ret;
}

/*
Function to store chunks in chunk store and hash in hash store.
Input:char *buff,char *hash,int length,int h_length,int e_offset,
int b_offset,int fd_stub
Output:int
*/
int 
chunk_store(char *buff,char *hash,int length,int h_length,int e_offset,
int b_offset,int fd_stub)
{

	int fd_hash	    =	-1;
	int fd_block	=	-1;
	int off			=	-1;
	int ret			=	-1;
	fd_hash =open("hashstore.txt",O_APPEND|O_CREAT|O_RDWR);
	if (fd_hash< 1)
	{
		printf("\nCreation of hash store failed with error [%s]\n",
		strerror(errno));
		goto out;
	}
	fd_block =open("blockstore.txt",O_APPEND|O_CREAT|O_RDWR);
	if (fd_block< 1)
	{
		printf("\nCreation of block file failed with error [%s]\n",
		strerror(errno));
		goto out;
	}
	ret=searchhash(hash,fd_hash);
	if (ret== -1)
	{
		goto out;
	}
	if (ret== 0)
	{
		printf("\nHash is already present");
		ret=searchstubhash(fd_stub, b_offset,e_offset);
		if (ret== -1)
		goto  out;
		if (ret== 1)
		{
			ret=write_to_stub(hash,h_length,fd_stub,b_offset,
			e_offset);
			if (ret== -1)
				goto out;
		}
	}
	else
	{
		off=write_to_block(buff, length,fd_block);
		if (off== -1)
		{
			printf("\nError in block generation\n");
			goto out;
		}
		ret=write_hash(hash,fd_hash,off);
		if (ret== -1)
		{
			printf("\nError in hash generation\n");
			goto out;
		}
		ret=searchstubhash(fd_stub, b_offset,e_offset);
		if (ret== -1)
		goto  out;
		if (ret== 1)
		{
			ret=write_to_stub(hash,h_length,fd_stub,b_offset,
			e_offset);
			if (ret== -1)
			{
				printf("\nError in stub file generation\n");
				goto out;
			}
		}
	}
	ret=0;
out:
	return ret;
}

/*Function to check whether second argument exist in directory or not.
Input:char *filename
Output:int
*/
inline int 
file_exist(char *filename)
{
	struct stat   buffer;
	return (stat (filename, &buffer) == 0);
}


