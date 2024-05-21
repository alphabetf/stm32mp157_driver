#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>

#define CMD_TRIG  100
static int fd;

/* ./sr04 /dev/sr04 
 */
int main(int argc, char** argv)
{
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
		ioctl(fd, CMD_TRIG);
		printf("I am goning to read distance: \n");
		if(read(fd, &val, 4) == 4){
			printf("get distance: %d cm\n",val*17/1000000);
		}else{
			printf("get distance err");
		}
		sleep(1);
	}

	close(fd);

	return 0;
}
