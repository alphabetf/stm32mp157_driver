#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* ./sr501_test /dev/sr501
 */
int main(int argc, char** argv)
{
	int fd;
	int val;

	if(argc != 2){
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if(fd < 0){
		printf("can not open file %s\n", argv[1]);
		return -1;
	}

	while(1){
		if(read(fd, &val, 4) == 4){
			printf("get sr501：0x%x\n", val);
		}else{
			printf("get sr501: -1\n");
		}
	}
	
	close(fd);

	return 0;
}
