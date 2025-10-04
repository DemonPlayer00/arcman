#include <iostream>
#include "handler.hpp"

using HandlerFunc = void(*)(const char*, const char*);

void getTargets(int argc, char* argv[], const char** input, const char** output) {
	*input = argv[2];
	if (argc >= 4)*output = argv[3];
}

int main(int argc, char* argv[]) {
	using namespace std;
	if (argc < 3) {
		cerr << "Usage:" << argv[0] << " [unpack|pack] [input] <output>" << endl;
		exit(1);
	}

	HandlerFunc handler_func = nullptr;
	const char* input = nullptr;
	const char* output = "output";

	if(strcmp(argv[1], "unpack")==0){
		handler_func = handler::unpack;
		getTargets(argc, argv, &input, &output);
	}
	else if (strcmp(argv[1], "pack")==0) {
		handler_func = handler::pack;
		getTargets(argc, argv, &input, &output);
	}
	else {
		cerr << "Usage:" << argv[0] << " [unpack|pack] [input] <output>" << endl;
		exit(1);
	}
	if(handler_func)handler_func(input, output);
}