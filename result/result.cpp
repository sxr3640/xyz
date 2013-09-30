#include "../libs/middle.h"


void readFrom(char* file, int **res, int *id){
	int fd = open(file);
	char * tmp = readline(fd);
	id = char2int(tmp);
	int i = 0;
	while((tmp = readline(fd)) != NULL){
		res[0] = char2array(tmp);
	}
}

// result -f xx.txt
int main(int argc, char ** argv){
	int res[][];
	int testID;
	readFrom(argv[2], res, &testID);
	int path[] = getTestInfo(testID);
	insert(res, path);
	bool beforeIVI = true;
	int problem[];
	for i from 0 : res.height
		beforeIVI = true;
		for j from 0 : res.width
			if(res[i][j] != JUSTLIKETHIS && beforeIVI)
				break;
			else if(res[i][j] != JUSTLIKETHIS && !beforeIVI)
				problem[i] = j;
			beforeIVI = false;
}
