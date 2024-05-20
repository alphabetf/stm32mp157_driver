#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>

static int fd;

/*
 * ./led /dev/myled <0|1> on	// led on
 * ./led /dev/myled <0|1> off	// led off
 * ./led /dev/myled <0|1> 		// led status
 */
int main(int argc, char** argv)
{
	int ret;
	char buf[2]; /* buf[0]:which led, buf[1]:on or off */
	
	if(argc < 3){
		printf("Usage:%s %s <0|1> [on|off]\n", argv[0], argv[1]);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if(fd == -1){
		printf("can't not open file %s\n", argv[1]);
		return -1;
	}
	if(argc == 4){ /* set led on or off */
		buf[0] = strtol(argv[2], NULL, 10);
		if(strcmp(argv[3], "on") == 0){	/* len on */
			buf[1] = 1;
		}else{		/* led off */
			buf[1] = 0;
		}

		ret = write(fd, buf, 2);
	}else{	/* read led status */
		buf[0] = strtol(argv[2], NULL, 10);
		ret = read(fd, buf, 2);
		if(ret == 2){
			printf("led %d status is %s \n", buf[0], buf[1]==0?"off":"on");
		}
	}

	close(fd);

	return 0;
}





