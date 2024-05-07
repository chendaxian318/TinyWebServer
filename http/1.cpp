
#include <sys/types.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include<iostream>

using namespace std;

int main() {

	char *s1 = (char*)"123123123123";
	char* s=NULL;
	s = strpbrk(s1, "3");
	if (!s) {
		std::cout << "test111\n";
		return 0;
	}
	std::cout << "test\n";
	//*s++ = '1';
	std::cout << "test\n";
	std::cout << s << std::endl;
	return 0;
}
