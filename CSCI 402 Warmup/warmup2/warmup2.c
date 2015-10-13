#include<time.h>
#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<stdarg.h>
#include<sys/time.h>
#include<string.h>
#include<signal.h>
#include<math.h>
#include"cs402.h"
#include"my402list.h"

#ifndef FAILANDEXIT
#define FAILANDEXIT(exp, ...)\
if(!(exp)){\
        fprintf(stderr, "error at source file %s line%d:\n", __FILE__, __LINE__);\
        fprintf(stderr, __VA_ARGS__);\
        exit(-1);\
}
#endif

#ifndef SYNCHRONIZED
#define SYNCHRONIZED
#endif

extern void (*sigset(int, void (*)(int)))(int); 

typedef struct{//all time represented by second
	double assumearrivetime;
	double assumeservicetime;
	double arrivetime;
	double interarrival;
	double timeenterQ1;
	double timeenterQ2;
	double timeleaveQ1;
	double timeleaveQ2;
	double timeenterserver;
	double timedepartserver;
	int id;
	int tokenneed;
}package_t;

static pthread_mutex_t g_mutex;
static pthread_cond_t server_cond;
static My402List g_Q1;
static My402List g_Q2;
static int g_tokennumber = 0;
static int g_isCanceled = FALSE;
static int g_numpackagearrived = 0;

static int g_const_bucketdepth = 10;
static double g_lambda = 1;
static double g_rate = 1.5;
static double g_mu = 0.35;
static int g_packageneed = 3;
static int g_packagenum = 20;
static char g_tsfilename[128];
static FILE* g_tsfile = NULL;

//for statistic
static double g_totaltime_interarrival = 0.0;
static double g_totaltime_service = 0.0;
static double g_totaltime_system = 0.0;
static double g_totalsquaretime_system = 0.0;
static double g_sd_system = 0.0;
static double g_time_inQ1 = 0.0;
static double g_time_inQ2 = 0.0;
static double g_time_inS1 = 0.0;
static double g_time_inS2 = 0.0;
static double token_drop_probability = 0;
static int g_packdropped = 0;
static int g_tokenarrived = 0;
static int g_tokendropped = 0;
static double packet_drop_probability = 0;

static
double CurrentMicrosec()
{
	struct timeval cur;
    gettimeofday(&cur , NULL);
    return (double)cur.tv_sec*1000000 + (double)cur.tv_usec;
}

static
double GetMillisecTimestamp()
{
	static int beginStamp = FALSE;
	static double begintime = 0.0;
	if(beginStamp == TRUE){
		double cur = CurrentMicrosec();
		return (cur - begintime)*0.001;
	}
	begintime = CurrentMicrosec();
	beginStamp = TRUE;
	return 0.0;
}

static 
void PrintfWithTimeStamp(double timestamp, const char* format, ...) SYNCHRONIZED
{
	static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
	FILE* file = stdout;
	va_list argList;
    va_start(argList, format);
   	pthread_mutex_lock(&print_mutex);
    fprintf(file, "%012.4fms: ", timestamp);	
    vfprintf(file, format, argList);
    pthread_mutex_unlock(&print_mutex);
    va_end(argList);
    fflush(file);
}

void MillisecSleep(double millisec)
{
	struct timeval tv;
	tv.tv_sec = (int)(millisec/1000.0);
	tv.tv_usec = 1000.0L*(millisec - tv.tv_sec*1000.L);
	select(1, NULL, NULL, NULL, &tv);
}

static 
void EnqueueQ1(package_t* pack) SYNCHRONIZED
{	
	extern My402List g_Q1;
	pack->timeenterQ1 = GetMillisecTimestamp();
	FAILANDEXIT(TRUE == My402ListAppend(&g_Q1, pack), "fail to enqueue\n");
	PrintfWithTimeStamp(pack->timeenterQ1, "p%d enters Q1\n", pack->id);
}

static 
void EnqueueQ2(package_t* pack) SYNCHRONIZED
{	
	extern My402List g_Q2;
	pack->timeenterQ2 = GetMillisecTimestamp();
	FAILANDEXIT(TRUE == My402ListAppend(&g_Q2, pack), "fail to enqueue\n");
	PrintfWithTimeStamp(pack->timeenterQ2, "p%d enters Q2\n", pack->id);
	pthread_cond_broadcast(&server_cond);
}

static 
package_t* Dequeue(My402List* queue, char* name) SYNCHRONIZED
{
	
	My402ListElem* first = My402ListFirst(queue);
	package_t* pack = (package_t*)first->obj;
	My402ListUnlink(queue, first);
	return pack;
}

static 
package_t* DequeueQ1() SYNCHRONIZED
{	
	extern My402List g_Q1;
	
	double timestamp = GetMillisecTimestamp();
	package_t* pack = Dequeue(&g_Q1, "Q1");
	extern int g_tokennumber;
	g_tokennumber -= pack->tokenneed;
	pack->timeleaveQ1 = timestamp;	
	PrintfWithTimeStamp(pack->timeleaveQ1, "p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d %s\n", 
						pack->id, pack->timeleaveQ1 - pack->timeenterQ1,  g_tokennumber, (g_tokennumber > 1)?"tokens":"token");
	return pack;
}

static 
package_t* DequeueQ2() SYNCHRONIZED
{	
	extern My402List g_Q2;
	double timestamp = GetMillisecTimestamp();
	package_t* pack = Dequeue(&g_Q2, "Q2");
	pack->timeleaveQ2 = timestamp;
	PrintfWithTimeStamp(pack->timeleaveQ2, "p%d leaves Q2, time in Q2 = %.3fms\n",
					 	pack->id, pack->timeleaveQ2 - pack->timeenterQ2 );
	return pack;
}

static 
int CanMovePackQ1toQ2() SYNCHRONIZED
{
	extern int g_tokennumber;
	extern My402List g_Q1;
	if(!My402ListEmpty(&g_Q1)){
		My402ListElem* first = My402ListFirst(&g_Q1);
		package_t* pack = (package_t*)first->obj;
		if(g_tokennumber >= pack->tokenneed){
			return TRUE;
		}
	}
	return FALSE;
}

static 
void TryMovePackQ1toQ2() SYNCHRONIZED
{
	if(CanMovePackQ1toQ2() == TRUE){
		EnqueueQ2(DequeueQ1());
	}
}

static
int CanTokenRun(){
	extern int g_isCanceled;
	extern int g_numpackagearrived;
	extern My402List g_Q1;
	return  !(g_isCanceled 
				|| ( (g_numpackagearrived >= g_packagenum) 
					 && My402ListEmpty(&g_Q1) )
			);
}

static 
int WaitTokenArrive()
{	
	static int tokenid = 0;
	extern double g_rate;
	double sleeptime = 1000.0/g_rate;
	if(sleeptime > 10000.0) sleeptime = 10000.0;
	double beforewait = GetMillisecTimestamp();
	static double afterprewait = 0.0;
	double delay = beforewait - afterprewait;
	if(sleeptime > delay){
		MillisecSleep(sleeptime - delay);
	}
	afterprewait = GetMillisecTimestamp();
	return ++tokenid;
}

static 
void DropToken(double timestamp, int tokenid) SYNCHRONIZED
{
	extern int g_tokendropped;
	++g_tokendropped;
	PrintfWithTimeStamp(timestamp, "token t%d arrives, dropped\n", tokenid);
	
}

static 
int TokenArrive(int tokenid) SYNCHRONIZED
{
	extern int g_const_bucketdepth;
	extern int g_tokennumber;
	extern int g_tokenarrived;
	++g_tokenarrived;
	double timestamp = GetMillisecTimestamp();
	if(g_tokennumber < g_const_bucketdepth){
		++g_tokennumber;
		PrintfWithTimeStamp(timestamp, "token t%d arrives, token bucket now has %d %s\n"
							, tokenid, g_tokennumber,  (g_tokennumber > 1)?"tokens":"token");
		return TRUE;
	}else{
		DropToken(timestamp, tokenid);
		return FALSE;
	}
}


static
void* TokenRun(void* arg)
{
	extern pthread_mutex_t g_mutex;
	while(CanTokenRun()){
		int tokenid = WaitTokenArrive();
		pthread_mutex_lock(&g_mutex);
		if(!CanTokenRun()){
			pthread_mutex_unlock(&g_mutex);
			break;
		}
		if(TRUE == TokenArrive(tokenid)){
			TryMovePackQ1toQ2();
		}
		extern int g_tokendropped;
		pthread_mutex_unlock(&g_mutex);
	}
	//wakeup server thread
	pthread_cond_broadcast(&server_cond);
	return NULL;
}

static 
int CanPackageRun()
{
	return (g_numpackagearrived < g_packagenum && !g_isCanceled) ? TRUE : FALSE;
}

static 
void CheckLine(char* line)
{
	FAILANDEXIT(line[0] <= '9' && line[0] >= '0', "invalid line\n");
	char test[128]; 
	int num = sscanf(line, "%s%*[ \t]%s%*[ \t]%s%[^\n]", test, test, test, test);
	FAILANDEXIT(3 == num, "invalid line\n");
}

static 
package_t* WaitPackageArrive()
{
	static int packageid = 0;
	extern FILE* g_tsfile;
	package_t* pack = (package_t*)malloc(sizeof(package_t));
	FAILANDEXIT(pack, "fail to malloc package\n");
	pack->id = ++packageid;	
	if(g_tsfile == NULL){
		extern double g_lambda;
		extern int g_packageneed;
		extern double g_mu;
		pack->tokenneed = g_packageneed;
		pack->assumearrivetime = 1000.0/g_lambda;
		pack->assumeservicetime = 1000.0/g_mu;
		if(pack->assumearrivetime > 10000.0){
			pack->assumearrivetime = 10000.0;					
		}
		if(pack->assumeservicetime > 10000.0){
			pack->assumeservicetime = 10000.0;					
		}
	}else{
		char line[128];
		FAILANDEXIT(fgets( line, sizeof(line), g_tsfile) != NULL, "invalid line\n");
		CheckLine(line);
		int assumearrivetime = 0;
		int assumeservicetime = 0;
		int num = sscanf(line, "%d%*[ \t]%d%*[ \t]%d", &assumearrivetime, &pack->tokenneed, &assumeservicetime);
		FAILANDEXIT(3 == num, "invalid line \n");
		pack->assumearrivetime = (double)assumearrivetime;
		pack->assumeservicetime = (double)assumeservicetime;	
	}
	static double afterprewait = 0.0;
	double beforewait = GetMillisecTimestamp();
	double delay = beforewait - afterprewait;
	if(pack->assumearrivetime > delay){
		MillisecSleep(pack->assumearrivetime - delay);
	}
	afterprewait = GetMillisecTimestamp();
	return pack;
}

static 
void DropPackage(package_t* pack) SYNCHRONIZED
{
	
	extern int g_packdropped;
	PrintfWithTimeStamp(pack->arrivetime, "p%d arrives, needs %d tokens, inter-arrival time = %.3fms, dropped\n",
						pack->id, pack->tokenneed, pack->interarrival);
	++g_packdropped;
	free(pack);
}

static 
int PackageArrive(package_t* pack) SYNCHRONIZED
{	
	extern int g_const_bucketdepth;
	extern double g_totaltime_interarrival;
	extern int g_packdropped;
	extern int g_numpackagearrived;
	double timestamp = GetMillisecTimestamp();
	
	static double lasttime = 0.0;
	pack->arrivetime = timestamp;
	pack->interarrival = pack->arrivetime - lasttime;
	lasttime = pack->arrivetime;
		
	g_totaltime_interarrival += pack->interarrival;
	if( CanPackageRun() ){
		++g_numpackagearrived;
		if(g_const_bucketdepth >= pack->tokenneed){
			PrintfWithTimeStamp(pack->arrivetime, "p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n",
								pack->id, pack->tokenneed, pack->interarrival);
			return TRUE;
		}
		DropPackage(pack);
	}
	return FALSE;
}

static
void* PackageRun(void* arg)
{
	extern pthread_mutex_t g_mutex;
	while(CanPackageRun()){
		package_t* pack = WaitPackageArrive();
		pthread_mutex_lock(&g_mutex);
		if(TRUE == PackageArrive(pack)){
			EnqueueQ1(pack);
			TryMovePackQ1toQ2();
		}
		pthread_mutex_unlock(&g_mutex);
	}
	pthread_cond_broadcast(&server_cond);
	return NULL;
}

static pthread_mutex_t statistic_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_completenum = 0;

static
void ServerPackage(package_t* pack, int serverid)
{
	
	pack->timeenterserver = GetMillisecTimestamp();
	PrintfWithTimeStamp(pack->timeenterserver, "p%d begins service at S%d, requesting %.3fms of service\n",
					 	pack->id, serverid, pack->assumeservicetime);
	MillisecSleep(pack->assumeservicetime);
	pack->timedepartserver = GetMillisecTimestamp();
	double time_service = pack->timedepartserver - pack->timeenterserver;
	double insystem_time = pack->timedepartserver - pack->arrivetime;
	PrintfWithTimeStamp(pack->timedepartserver,  "p%d departs from S%d, service time = %.3fms, time in system = %.3fms\n",
						pack->id, serverid, time_service, insystem_time);
	
	extern double g_totaltime_service;
	extern double g_totaltime_system;
	extern double g_time_inQ1;
	extern double g_time_inQ2;
	extern double g_time_inS1;
	extern double g_time_inS2;

	extern pthread_mutex_t statistic_mutex;
   	pthread_mutex_lock(&statistic_mutex);
   	
   	++g_completenum;
	g_totaltime_service += time_service;
	g_totaltime_system += insystem_time;
	
	extern double g_totalsquaretime_system;
	g_totalsquaretime_system += insystem_time * insystem_time;
	
	g_time_inQ1 += (pack->timeleaveQ1 - pack->timeenterQ1);
	g_time_inQ2 += (pack->timeleaveQ2 - pack->timeenterQ2);
	
	if(serverid == 1){
		g_time_inS1 += time_service;
	}
	else{
		g_time_inS2 += time_service;
	}
	free(pack);
    pthread_mutex_unlock(&statistic_mutex);
   
}

static
int CanServernRun(){
	extern int g_isCanceled;
	extern int g_numpackagearrived;
	extern My402List g_Q1;
	return  !(g_isCanceled 
				|| ( (g_numpackagearrived >= g_packagenum) 
					 	&& My402ListEmpty(&g_Q1) 
					 	&& My402ListEmpty(&g_Q2) 
					)
			);
}

static
void WaitItem() 
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	extern pthread_cond_t server_cond;
	//struct timespec timeout;
	//timeout.tv_nsec = 1000000.0L;
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&server_cond, &mutex);
	pthread_mutex_unlock(&mutex);
}

static
void* ServerRun(void* arg)
{
	extern pthread_mutex_t g_mutex;
	extern My402List g_Q2;
	int serverid = (int)arg;
	while(CanServernRun()){
		package_t* pack = NULL;
		pthread_mutex_lock(&g_mutex);
		if(!My402ListEmpty(&g_Q2)){
			pack = DequeueQ2();
		}
		pthread_mutex_unlock(&g_mutex);
		if(pack){
			ServerPackage(pack, serverid);
		}else{
			WaitItem();
		}
	}
	return NULL;
}

static
void Usage()
{
	fprintf(stderr, "Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
	exit(-1);
}

static 
void ParseCommand(int argc, char** argv)
{
	extern int g_const_bucketdepth;
	extern double g_lambda;
	extern double g_rate;
	extern double g_mu;
	extern int g_packageneed;
	extern int g_packagenum;
	extern FILE* g_tsfile;
	extern char g_tsfilename[128];
	if( (argc%2 == 0) || argc > 15)
		Usage();
	int num = 1;
	for(; num < argc; ++num){
		if(argv[num][0] != '-' || (num + 1) >= argc)
			Usage();
		if(0 == strncmp("-lambda", argv[num], 7) && argv[num][7] == '\0'){			
			sscanf(argv[++num], "%lf", &g_lambda);
			continue;
		}
		if(0 == strncmp( "-mu", argv[num], 3)  && argv[num][3] == '\0'){
			sscanf(argv[++num], "%lf", &g_mu);
			continue;
		}
		if(0 == strncmp("-r", argv[num], 2)  && argv[num][2] == '\0'){
			sscanf(argv[++num], "%lf", &g_rate);
			continue;
		}
		if(0 == strncmp("-B", argv[num], 2)  && argv[num][2] == '\0'){
			sscanf(argv[++num], "%d", &g_const_bucketdepth);
			FAILANDEXIT( g_const_bucketdepth <= 0x7fffffff && g_const_bucketdepth > 0,
			"The range of B is 1-2147483647\n");
			continue;
		}
		if(0 == strncmp("-P", argv[num], 2)  && argv[num][2] == '\0'){
			sscanf(argv[++num], "%d", &g_packageneed);
			FAILANDEXIT( g_packageneed <= 0x7fffffff && g_packageneed > 0,
			"The range of P is 1-2147483647\n");
			continue;
		}
		if(0 == strncmp("-n", argv[num], 2)  && argv[num][2] == '\0'){
			sscanf(argv[++num], "%d", &g_packagenum);
			FAILANDEXIT( g_packagenum <= 0x7fffffff && g_packagenum > 0,
			"The range of n is 1-2147483647\n");
			continue;
		}
		
		if(0 == strncmp("-t", argv[num], 2)  && argv[num][2] == '\0'){
			g_tsfile = fopen(argv[++num], "r");
			FAILANDEXIT(g_tsfile, "fail to open file %s\n", argv[num]);
			sscanf(argv[num], "%s",  g_tsfilename);
			FAILANDEXIT(1 == fscanf(g_tsfile, "%d\n", &g_packagenum), "invalid line\n");
			continue;
		}
		Usage();
	}
	//if(0 == strncmp( "", argv[num])){
	//	;
	//}
}

static sigset_t newsignet;

static
void CancelAllThread(int sig)
{
	extern sigset_t newsignet;
	extern int g_isCanceled;
	g_isCanceled = TRUE;
	pthread_cond_broadcast(&server_cond);
	pthread_sigmask(SIG_UNBLOCK, &newsignet, NULL);
}

static 
void PrintNaNInfo(int isNAN)
{
	if(isNAN == TRUE)
		fprintf(stdout, "(no packet arrived at this facility)");
	fprintf(stdout, "\n");
}

static
void PrintfStatistics(double emulationtime)
{
	extern double g_totaltime_interarrival;
	extern double g_totaltime_service;
	extern double g_totaltime_system;
	extern double g_time_inQ1;
	extern double g_time_inQ2;
	extern double g_time_inS1;
	extern double g_time_inS2;
	extern double g_sd_system;
	extern double token_drop_probability;
	extern int g_tokenarrived;
	extern double packet_drop_probability;
	extern int g_completenum;

	FILE* file = stdout;
	fprintf(file, "Statistics:\n\n");

    fprintf(file, "    average packet inter-arrival time = %.6g", 0.001 * g_totaltime_interarrival/g_numpackagearrived);
   	PrintNaNInfo(0 == g_numpackagearrived);
    fprintf(file, "    average packet service time = %.6g", 0.001 * g_totaltime_service/g_completenum);
    PrintNaNInfo(0 == g_completenum);
    fprintf(stdout, "\n");
    
    fprintf(file, "    average number of packets in Q1 = %.6g", g_time_inQ1/emulationtime);
    PrintNaNInfo(0 == emulationtime);
    fprintf(file, "    average number of packets in Q2 = %.6g", g_time_inQ2/emulationtime);
    PrintNaNInfo(0 == emulationtime);
    fprintf(file, "    average number of packets at S1 = %.6g", g_time_inS1/emulationtime);
    PrintNaNInfo(0 == emulationtime);
    fprintf(file, "    average number of packets at S2 = %.6g", g_time_inS2/emulationtime);
    PrintNaNInfo(0 == emulationtime);
    fprintf(stdout, "\n");
     
    fprintf(file, "    average time a packet spent in system = %.6g", 0.001 * g_totaltime_system/g_completenum);
    PrintNaNInfo(0 == g_completenum);
    
    g_sd_system = sqrt(g_totalsquaretime_system*g_completenum - g_totaltime_system * g_totaltime_system)/g_completenum;
    fprintf(file, "    standard deviation for time spent in system = %.6g", 0.001 * g_sd_system);
	PrintNaNInfo(0 == g_completenum);
	fprintf(stdout, "\n");
	
	token_drop_probability = 1.0*(double)g_tokendropped/(double)(g_tokenarrived);
	
	packet_drop_probability = 1.0*(double)g_packdropped/(double)g_numpackagearrived;
    fprintf(file, "    token drop probability = %.6g", token_drop_probability);
    PrintNaNInfo(0 == g_tokenarrived);
    fprintf(file, "    packet drop probability = %.6g", packet_drop_probability);
    PrintNaNInfo(0 == g_numpackagearrived);
    fflush(file);
}

static
void PrintfParameters()
{
	extern int g_const_bucketdepth;
	extern double g_lambda;
	extern double g_rate;
	extern double g_mu;
	extern int g_packageneed;
	extern int g_packagenum;
	extern char g_tsfilename[128];
	extern FILE* g_tsfile;
	
	FILE* file = stdout;
	fprintf(file, "Emulation Parameters:\n"); 
    fprintf(file, "    number to arrive = %d\n", g_packagenum);
   	if(!g_tsfile){
    	fprintf(file, "    lambda = %g\n", g_lambda);
    	fprintf(file, "    mu = %g\n", g_mu);
    }
    fprintf(file, "    r = %g\n", g_rate);
    fprintf(file, "    B = %d\n", g_const_bucketdepth);
  	if(!g_tsfile){
    	fprintf(file, "    P = %d\n", g_packageneed);
   	}
    if(g_tsfile){
    	fprintf(file, "    tsfile = %s \n",  g_tsfilename);
    }
    fprintf(file, "\n");
    fflush(file);
}


static 
void CleanList(My402List* list, char* name)
{
	while(!My402ListEmpty(list)){
		My402ListElem* first = My402ListFirst(list);
		package_t* pack = (package_t* )first->obj;
		PrintfWithTimeStamp(GetMillisecTimestamp(), "p%d removed from %s\n", pack->id, name);
		free(first->obj);
		My402ListUnlink(list, first);
		  
	}
}

int main(int argc, char** argv)
{
	extern sigset_t newsignet;
	ParseCommand(argc, argv);
	PrintfParameters();
	sigemptyset(&newsignet);
	sigaddset(&newsignet, SIGINT);
	pthread_sigmask(SIG_BLOCK, &newsignet, NULL);
	sigset(SIGINT, CancelAllThread);
	extern pthread_mutex_t g_mutex;
	pthread_mutex_init(&g_mutex, NULL);
	FAILANDEXIT(0 == pthread_cond_init(&server_cond, NULL), "fail to initialize mutex!\n");
	extern pthread_cond_t server_cond;
	FAILANDEXIT(0 == pthread_cond_init(&server_cond, NULL), "fail to initialize condtion variable!\n");
	extern My402List g_Q1;
	extern My402List g_Q2;
	My402ListInit(&g_Q1);
	My402ListInit(&g_Q2);
	pthread_t packagethread = 0;
	pthread_t tokenthread = 0;
	pthread_t serverthread1 = 0;
	pthread_t serverthread2 = 0;
	double begin = GetMillisecTimestamp();
	PrintfWithTimeStamp(begin , "emulation begins\n");
	FAILANDEXIT(pthread_create(&packagethread, NULL, PackageRun, NULL) == 0, "package thread create error!\n");
	FAILANDEXIT(pthread_create(&tokenthread, NULL, TokenRun, NULL) == 0, "token thread create error!\n");
	FAILANDEXIT(pthread_create(&serverthread1, NULL, ServerRun, (void*)1) == 0, "server1 thread create error!\n");
	FAILANDEXIT(pthread_create(&serverthread2, NULL, ServerRun, (void*)2) == 0, "server2 thread create error!\n");
	pthread_join(packagethread, NULL);
	pthread_join(tokenthread, NULL);
	pthread_join(serverthread1, NULL);
	pthread_join(serverthread2, NULL);
	CleanList(&g_Q1, "Q1");
	CleanList(&g_Q2, "Q2");
	double end = GetMillisecTimestamp();
	PrintfWithTimeStamp(end, "emulation ends\n\n");
	PrintfStatistics(end - begin);
	pthread_mutex_destroy(&g_mutex); 
	pthread_cond_destroy(&server_cond);
	extern FILE* g_tsfile;
	if(g_tsfile){
		fclose(g_tsfile);
	}
    return 0;
}
