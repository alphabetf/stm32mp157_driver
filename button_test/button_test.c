#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

static int fd;

/*
 * ./button /dev/mybutton0
 */
int main(int argc, char** argv)
{
	int val;
	int flags;

	if(argc != 2){
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}

	/* open file */
	fd = open(argv[1], O_RDWR|O_NONBLOCK);
	if(fd == -1){
		printf("cant not open file %s\n", argv[1]);
		return -1;
	}

	for(int i = 0; i < 10; i++){
		if(read(fd, &val, 4) == 4){
			printf("get button:0x%x\n", val);
		}else{
			printf("get button: -1\n");
		}
	}

	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags &~ O_NONBLOCK);

	while(1){
		if(read(fd, &val, 4) == 4){
			printf("get button:0x%x\n", val);
		}else{
			printf("while get button:-1\n");
		}
	}

	close(fd);

	return 0;
}





