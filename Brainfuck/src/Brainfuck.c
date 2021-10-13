/*
 ============================================================================
 Name        : Hello.c
 Author      : bsoelch
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

static const int LOOP_STACK_INIT_CAPACITY = 8;
static const int  TAPE_INIT_SIZE =128;

static const int  FILE_SEEK_ERROR =-2;

static const int  STATE_RUNNING =0;
static const int  STATE_ERROR =-1;
static const int  STATE_FINISHED =1;

static const int  DEBUG_BREAK_CHAR =1;
static const int  DEBUG_BREAK_STEPS =2;
static const int  DEFAULT_TAPE_PRINT_WIDTH =5;

typedef struct{
	size_t codePos;

	int* tape;
	size_t tapeCapacity;
	size_t tapePos;

	size_t* loopStack;
	size_t loopStackCapacity;
	size_t loopStackSize;

	int state;

} CodeState;

typedef struct{
	int flags;

	char breakChar;
	int stepsRemaining;

}DebugInfo;


CodeState* newCodeState(){
	CodeState* create=malloc(sizeof(CodeState));
	if(create==NULL)
		return NULL;
	create->codePos=0;
	create->tape = calloc(TAPE_INIT_SIZE,sizeof(int));
	if(create->tape==NULL){
		free(create);
		return NULL;
	}
	create-> tapeCapacity=TAPE_INIT_SIZE;
	create-> tapePos=TAPE_INIT_SIZE/2;
	create -> loopStack = malloc(sizeof(size_t)*LOOP_STACK_INIT_CAPACITY);
	if(create -> loopStack==NULL){
		free(create -> tape);
		free(create);
		return NULL;
	}
	create -> loopStackSize=0;
	create -> loopStackCapacity=LOOP_STACK_INIT_CAPACITY;

	create -> state=STATE_RUNNING;
	return create;
}

//free a CodeState and all its contents
void freeCodeState(CodeState* toFree){
	if(toFree!=NULL){
		free(toFree->tape);
		free(toFree->loopStack);
		free(toFree);
	}
}

void run(char* program,int size,CodeState* state,DebugInfo* debug){
	if(state==NULL){
		printf("The supplied codeState was NULL");
		return;
	}
	bool moved=false;
	for(;state->codePos<size;state->codePos++){
		if(debug){
			if(moved&&(((debug->flags)&DEBUG_BREAK_CHAR)!=0)){
				if(program[state->codePos]==debug->breakChar){
					return;//break on char
				}
			}//no else
			if(((debug->flags)&DEBUG_BREAK_STEPS)!=0){
				debug->stepsRemaining--;
				if(debug->stepsRemaining<0){
					return;//break after given number of steps
				}
			}
			moved=true;
		}
		if(program[state->codePos]=='<'){//move left
			state->tapePos--;
			if(state->tapePos<0){
				int* newTape=calloc(2*state->tapeCapacity,sizeof(int));
				if(newTape==NULL){
					printf("Out of Memory");
					state -> state =STATE_ERROR;
					return;
				}
				memcpy(newTape+state->tapeCapacity,state->tape,sizeof(int)*state->tapeCapacity);
				state->tapePos+=state->tapeCapacity;//update position
				state->tapeCapacity=2*state->tapeCapacity;
				free(state->tape);
				state->tape=newTape;
			}
		}else if(program[state->codePos]=='>'){//move right
			state->tapePos++;
			if(state->tapePos>=state->tapeCapacity){
				state->tape=realloc(state->tape,sizeof(int)*2*state->tapeCapacity);
				if(state->tape==NULL){
					printf("Out of Memory");
					state -> state =STATE_ERROR;
					return;
				}
				memset(state->tape+state->tapeCapacity,0,sizeof(int)*state->tapeCapacity);
				state->tapeCapacity=2*state->tapeCapacity;
			}
		}else if(program[state->codePos]=='+'){//increment cell
			state->tape[state->tapePos]++;
		}else if(program[state->codePos]=='-'){//decrement cell
			state->tape[state->tapePos]--;
		}else if(program[state->codePos]=='.'){//out
			printf("%c",(char)state->tape[state->tapePos]);
		}else if(program[state->codePos]==','){//in
			if(debug){
				printf("Input Char:\n");
			}
			fflush(stdout);
			state->tape[state->tapePos]=getchar();
		}else if(program[state->codePos]=='['){//start loop
			if(state->tape[state->tapePos]==0){//skip loop
				int bracketCount=0;
				//skip  to matching ]
				for(;state->codePos<size;state->codePos++){
					if(program[state->codePos]=='['){
						bracketCount++;
					}else if(program[state->codePos]==']'){
						bracketCount--;
						if(bracketCount==0){
							break;
						}
					}
				}
				if(bracketCount!=0){
					printf("missing closing bracket\n");
					state->state=STATE_ERROR;
					return;
				}
			}else{
				if(state->loopStackSize==state->loopStackCapacity){
					//stack if full => reallocate with twice the current size
					state->loopStack=realloc(state->loopStack,
							sizeof(size_t)*2*state->loopStackCapacity);
					if(state->loopStack==NULL){
						printf("Out of Memory");
						state -> state =STATE_ERROR;
						return;
					}
					state->loopStackCapacity=2*state->loopStackCapacity;
				}
				state->loopStack[state->loopStackSize++]=state->codePos;
			}
		}else if(program[state->codePos]==']'){//end loop
			if(state->tape[state->tapePos]!=0){//jump to start if nonzero
				if(state->loopStackSize>0){
					state->codePos=state->loopStack[state->loopStackSize-1];//jump to start of loop
				}else{
					printf("unexpected closing bracket at position:%zu\n",state->codePos);
					state -> state =STATE_ERROR;
					return;
				}
			}else{
				state->loopStackSize--;//remove top loop
			}
		}
	}
	state -> state = STATE_FINISHED;
	return;
}



void printDebugCommandInfo() {
	puts("Input a Command:");
	puts("breakAt[Char] \t to define the given char as the breakpoint-char");
	puts("step \t to execute one instruction");
	puts("step:[N] \t to execute up [N] instructions");
	puts("run \t to execute to the next breakpoint or the end of program");
	puts("state \t to print the current state of the program");
	puts("tape:[N] \t to the tape surrounding the current position +-[N] cells");
}

#define MAX_DEBUG_ARGS   10
#define MAX_DEBUG_ARG_LEN   15

void printTape(CodeState* state,int width){
	if(state!=NULL&&state->tape!=NULL){
		int p=state->tapePos-width,max=state->tapePos+width;
		fputs("... ",stdout);
		while(p<0){
			p++;
			fputs("0 ",stdout);
		}
		while(p<=max&&p<state->tapeCapacity){
			if(p==(state->tapePos)){
				printf("[%i] ",state->tape[p++]);
			}else{
				printf("%i ",state->tape[p++]);
			}
		}
		while(p++<=max){
			fputs("0 ",stdout);
		}
		puts("...");
	}else{
		puts("NULL");
	}
}

void printState(CodeState* state){
	if(state!=NULL){
		printf("codePos: %zu\n",state->codePos);
		printf("tape: ");
		printTape(state, DEFAULT_TAPE_PRINT_WIDTH);
		printf("tapePos: %zu\n",state->tapePos);
		printf("loopStack: ");
		if(state->loopStack==NULL){
			printf("NULL\n");
		}else{
			printf("[");
			for(int i=0;i<state->loopStackSize;i++){
				printf(i==0?"%zu":", %zu",state->loopStack[i]);
			}
			printf("]\n");
		}
		printf("state: ");
		if(state->state ==STATE_RUNNING){
			printf("running\n");
		}else if(state->state==STATE_ERROR){
			printf("error\n");
		}else if(state->state==STATE_FINISHED){
			printf("finished\n");
		}else{
			printf("%i\n",state->state);
		}
	}else{
		printf("NULL\n");
	}
}

void executeDebugStep(char* program,int size,CodeState* state){
	putchar('>');
	fflush(stdout);
	//MAX_DEBUG_ARG_LEN+2: +1 for overflow char, +1 for 0-terminated
	char buffer[MAX_DEBUG_ARGS][MAX_DEBUG_ARG_LEN+2] = { 0 };
	int lens [MAX_DEBUG_ARGS]={0};
	int args=0;
	int r;
	while((r=getchar())!=EOF){
		if(r=='\n'||r=='\r'){
			if(lens[args]>0){
				args++;
			}
			break;
		}else{
			if(isspace(r)){
				if(lens[args]>0){
					args++;
					if(args>=MAX_DEBUG_ARGS){
						fprintf(stderr,"To many debug arguments, max allowed number:%i\n",
								MAX_DEBUG_ARGS);
						fflush(stderr);
						printDebugCommandInfo();
						return;
					}
				}
			}else{
				buffer[args][lens[args]++]=r;
				if(lens[args]>MAX_DEBUG_ARG_LEN){
					fprintf(stderr,"To argument %s is to long, max allowed length:%i\n",
							buffer[args],MAX_DEBUG_ARG_LEN);
					fflush(stderr);
					printDebugCommandInfo();
					return;
				}
			}
		}
	}
	if(args==0){
		return;//empty input
	}
	DebugInfo debug={0};
	_Bool doPrintState=0;
	_Bool doRun=0;
	int printTapeWidth=-1;
	for(int i=0;i<args;i++){
		if(strncmp(buffer[i],"breakAt",7)==0){//breakAt
			debug.flags|=DEBUG_BREAK_CHAR;
			doRun=1;
			if(lens[i]==7){
				debug.breakChar=' ';
			}else if(buffer[i][7]=='\\'){
				if(lens[i]==8){
					debug.breakChar=' ';
				}else{//XXX more escape chars
					switch(buffer[i][8]){
					case 'n':
						debug.breakChar='\n';break;
					case 't':
						debug.breakChar='\t';break;
					case 'r':
						debug.breakChar='\r';break;
					case '\\':
						debug.breakChar='\\';break;
					default:
						debug.breakChar=buffer[i][8];break;
					}
				}
			}else{
				debug.breakChar=buffer[i][7];
			}
		}else if(strncmp(buffer[i],"step",4)==0){//step step:[N]
			debug.flags|=DEBUG_BREAK_STEPS;
			doRun=1;
			if(lens[i]==4){
				debug.stepsRemaining++;
			}else if(buffer[i][4]==':'){
				char* tail=buffer[i]+lens[i];
				long toInt=strtol(buffer[i]+5,&tail,10);
				if(tail==buffer[i]+lens[i]){//sucessFull
					debug.stepsRemaining+=toInt;
				}else{
					fprintf(stderr,"%s is not a valid number of steps\n",buffer[i]+5);
					fflush(stderr);
					printDebugCommandInfo();
					return;
				}
			}else{
				fprintf(stderr,"invalid syntax for step %s\n",buffer[i]);
				fflush(stderr);
				printDebugCommandInfo();
				return;
			}
		}else if(strcmp(buffer[i],"run")==0){//run
			doRun=1;
		}else if(strcmp(buffer[i],"state")==0){//state
			doPrintState=1;
		}else if(strncmp(buffer[i],"tape",4)==0){//tape:[N]
			if(lens[i]==4){
				printTapeWidth=DEFAULT_TAPE_PRINT_WIDTH;
			}else if(buffer[i][4]==':'){
				char* tail=buffer[i]+lens[i];
				long toInt=strtol(buffer[i]+5,&tail,10);
				if(tail==buffer[i]+lens[i]){//sucessFull
					printTapeWidth=toInt;
				}else{
					fprintf(stderr,"%s is not a valid tapeWidth\n",buffer[i]+5);
					fflush(stderr);
					printDebugCommandInfo();
					return;
				}
			}else{
				fprintf(stderr,"invalid syntax for tape: %s\n",buffer[i]);
				fflush(stderr);
				printDebugCommandInfo();
				return;
			}
		}else{
			if(strcmp(buffer[i],"help")!=0){
				fprintf(stderr,"%s is not a valid command\n",buffer[i]);
				fflush(stderr);
			}
			printDebugCommandInfo();
			return;
		}
	}
	if(doRun){
		run(program,size,state,&debug);
	}
	if(doPrintState){
		printState(state);
	}
	if(printTapeWidth>=0){
		printTape(state,printTapeWidth);
	}
	fflush(stdout);
}

/* Copy from StackOverflow
 * fp is assumed to be non null
 * */
long int fsize(FILE *fp){
    long int prev=ftell(fp);
    if(fseek(fp, 0L, SEEK_END)!=0){
		return FILE_SEEK_ERROR;
    }
    long int sz=ftell(fp);
    //go back to where we were
    if(fseek(fp,prev,SEEK_SET)!=0){
		return FILE_SEEK_ERROR;
    }
    return sz;
}

int main(int numArgs,char** args) {
	printf("numArgs=%i\n",numArgs);
	for(int i=0;i<numArgs;i++){
		printf("args[%i]=%s\n",i,args[i]);
	}
	bool debug;
	char *fileName=NULL;
	if(numArgs==2){
		debug=false;
		fileName = args[1];
	}else if((strcmp(args[1],"-d")==0)&&(numArgs==3)){
		debug=true;
		fileName = args[2];
	}
	if(fileName==NULL){
		printf("usage: [Filename]\n or -d [Filename]");
		return EXIT_FAILURE;
	}

	FILE *file = fopen(fileName, "r");
	if(file!=NULL){
		int size=fsize(file);
		char* buffer;
		if(size==FILE_SEEK_ERROR){
			printf("IO Error while detecting file-size\n");
			return EXIT_FAILURE;
		}else if(size<0){//XXX recover form Undetected fileSize (if seek worked)
			printf("IO Error while detecting file-size\n");
			return EXIT_FAILURE;
		}else{
			printf("size of %s: %i\n",fileName,size);
			buffer = malloc(size*sizeof(char));
			if(buffer==NULL){
				printf("Memory Error\n");
				return EXIT_FAILURE;
			}
			size=fread(buffer,sizeof(char),size,file);
			if(size<0){
				printf("IO Error while reading file\n");
				free(buffer);
				return EXIT_FAILURE;
			}
			fclose(file);//file no longer needed (whole contents are buffered)
			buffer = realloc(buffer,(size+1)*sizeof(char));
		}
		if(buffer==NULL){
			printf("Memory Error\n");
			return EXIT_FAILURE;
		}
		//For debugging
		buffer[size]='\0';//null-terminate string (for printf)
		printf("Contents of File:\n%s\n",buffer);
		CodeState* state=newCodeState();
		if(state==NULL){
			printf("Out of Memory");
			free(buffer);
			return EXIT_FAILURE;
		}
		if(debug){
			printf("Debugging %s\n",fileName);
			do{
				executeDebugStep(buffer,size,state);
			}while(state->state==STATE_RUNNING);
		}else{
			run(buffer,size,state,NULL);
		}
		freeCodeState(state);
		free(buffer);
		return EXIT_SUCCESS;
	}else{
		printf("IO Error while opening File: %s\n",fileName);
		return EXIT_FAILURE;
	}
}
