#include"cs402.h"
#include"my402list.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

typedef struct{
        unsigned long time;
        double amount;
        char description[128];
}transaction;
#define FAILANDEXIT(exp, ...)\
if(!(exp)){\
        fprintf(stderr, "error at source file %s line%d:\n", __FILE__, __LINE__);\
        fprintf(stderr, __VA_ARGS__);\
        exit(-1);\
}

static
My402ListElem* Merge(My402ListElem* first1, int num1, My402ListElem* first2, int num2)
{
        My402ListElem* p1 = first1;
        My402ListElem* p2 = first2;
        My402ListElem dummy;
        My402ListElem* cur = &dummy;
        int index1 = 0;
        int index2 = 0;
        while(index1 != num1 && index2 != num2){
                transaction* t1 = (transaction*)p1->obj;
                transaction* t2 = (transaction*)p2->obj;
                FAILANDEXIT( (t1->time != t2->time) , "error: find identical timestamp!\n");
                if(t1->time < t2->time){
                        cur->next = p1;
                        p1->prev = cur;
                        p1 = p1->next;
                        ++index1;
                }else{
                        cur->next = p2;
                        p2->prev = cur;
                        p2 = p2->next;
                        ++index2;
                }
                cur = cur->next;
        }
        if(index1 != num1){
                cur->next = p1;
                p1->prev = cur;
        }
        if(index2 != num2){
                cur->next = p2;
                p2->prev = cur;
        }
        return dummy.next;
}
static
My402ListElem* Mergesort(My402ListElem* first, int num)
{
        if(num <= 1) return first;
        int num1 = num/2;
        int num2 = num-num1;
        My402ListElem* first2 = first;
        int i = 0;
        for(; i != num1; ++i){
                first2 = first2->next;
        }
        My402ListElem* first1 = Mergesort(first, num1);
        first2 = Mergesort(first2, num2);
        return Merge(first1, num1, first2, num2);
}
static
void sort(My402List* list)
{
        if(list->num_members <= 1) return;
        My402ListElem* first = Mergesort(list->anchor.next, list->num_members);
        list->anchor.next = first;
        first->prev = &(list->anchor);
        My402ListElem* cur = first;
        int i = 1;
        for(; i != list->num_members; ++i){
                cur = cur->next;
        }
        list->anchor.prev = cur;
        cur->next = &(list->anchor);
}
static
void Usage()
{
    fprintf(stderr,
            "usage: warmup1 sort [tfile]\n");
    exit(-1);
}

static
void CheckLine(char* line, int linenum )
{
        FAILANDEXIT( strchr(line, '\n') , "line is too long! line number = %d\n ", linenum);
        char* pos0 = strchr(line, '\t');
        FAILANDEXIT( pos0, "get invalid line! line number = %d\n ", linenum);
        char* pos1 = strchr(pos0+1, '\t');
        FAILANDEXIT( pos1, "get invalid line! line number = %d\n ", linenum);
        char* pos2 = strchr(pos1+1, '\t');
        FAILANDEXIT( pos2, "get invalid line! line number = %d\n ", linenum);
        char* pos3 = strchr(pos2+1, '\t');
        FAILANDEXIT( !pos3, "get invalid line! line number = %d\n ", linenum);
        FAILANDEXIT( (pos1 - pos0) < 12, "bad time! line number = %d\n ", linenum);
        FAILANDEXIT( (pos2 - pos1) < 12, "bad amount line number= %d\n ", linenum);
        FAILANDEXIT( (pos2[-3]) == '.', "bad amount line number = %d\n ", linenum);
}
static
transaction* Line2Transaction(char* line, int linenum)
{
        FAILANDEXIT( (line[0] == '-') || (line[0] == '+'), "the type of the transaction is wrong! type = %c\n", line[0]);
        CheckLine(line, linenum);
        transaction* t = (transaction*)malloc(sizeof(transaction));
        FAILANDEXIT(t,"malloc failed\n");
        char type = '\0';
        int numfilled = sscanf(line, "%c\t%lu\t%lf\t%[^\n]", &type, &t->time, &t->amount, t->description);
        FAILANDEXIT( numfilled == 4, "get invalid line! line number = %d\n ", linenum);
        FAILANDEXIT( time(NULL) >= t->time, "time exceed current time line number = %d\n ", linenum);
        if(type == '-') t->amount = -t->amount;
        return t;
}

static
int ReadListFromFile(FILE *file, My402List* list)
{
        char line[1025];
        int num = 0;
        while(fgets ( line, sizeof(line), file ) != NULL && line[0] != '\n'){
                transaction* t = Line2Transaction(line, num+1);
                (void)My402ListAppend(list, (void*)t);
                ++num;
        }
        FAILANDEXIT( fgets ( line, sizeof(line), file ) == NULL, "empty line! line number = %d\n ", num-1);
        return num;
}
void formatNumber(char* buf, double num)
{
        char sign = (num > 0)? '+' : '-';
        if(sign == '-') num = -num;
        sprintf(buf , "%*.2lf", 13, num);
        if(num >= 10000000.0l){
                sprintf(buf , " ??,???,???.?? ");
        }else{
                if(num >= 1000000.0l){
                        buf[0] = buf[0+2];
                        buf[1] = buf[1+2];
                        buf[2] = ',';
                }
                if(num >= 1000.0l){
                        buf[3] = buf[3+1];
                        buf[4] = buf[4+1];
                        buf[5] = buf[5+1];
                        buf[6] = ',';
                }
        }
        buf[13] = ' ';
        buf[14] = '\0';
        if(sign == '-'){
                buf[0] = '(';
                buf[13] = ')';
        }
}

static
double PrintTransaction(transaction* t, double balance)
{
        //struct tm *timeinfo = localtime ((time_t*)(&t->time));
        //char timebuffer[16];
        //strftime (timebuffer, 16, "%a %b %e %Y", timeinfo);
        char* timebuffer = ctime((time_t*)(&t->time));
        timebuffer[11] = timebuffer[20];
        timebuffer[12] = timebuffer[21];
        timebuffer[13] = timebuffer[22];
        timebuffer[14] = timebuffer[23];
        timebuffer[15] = '\0';
        char amountbuffer[16]={'0'};
        formatNumber(amountbuffer, t->amount);
        char balancebuffer[16]={'0'};
        balance = balance +  t->amount;
        formatNumber(balancebuffer, balance);
        char descriptionbuffer[32];
        //format description string
        sprintf(descriptionbuffer, "%-24.24s", t->description);
        fprintf(stdout, "| %s | %s | %s | %s |\n", timebuffer, descriptionbuffer, amountbuffer, balancebuffer);
        return balance;
}

static
void PrintList(My402List *pList, int num_items)
{
        fprintf(stdout, "+-----------------+--------------------------+----------------+----------------+\n");
        fprintf(stdout, "|       Date      | Description              |         Amount |        Balance |\n");
        fprintf(stdout, "+-----------------+--------------------------+----------------+----------------+\n");
        My402ListElem *elem=NULL;

    if (My402ListLength(pList) != num_items) {
        fprintf(stderr, "List length is not %1d in PrintTestList().\n", num_items);
        exit(1);
    }
    double balance = 0;
    for (elem=My402ListFirst(pList); elem != NULL; elem=My402ListNext(pList, elem)) {
        transaction* t=(transaction*)(elem->obj);
        balance = PrintTransaction(t, balance);
    }
    fprintf(stdout, "+-----------------+--------------------------+----------------+----------------+\n");
}

int main(int argc, char *argv[])
{
        if(argc <2 || argc>3){
                Usage();
        }
        if(strncmp("sort", argv[1], 4) ){
                Usage();
        }
        FILE* file = ((argc == 3 ) ? fopen(argv[2],"r") : stdin);
    FAILANDEXIT(file, "open file %s failed \n", argv[2]);
    My402List list;
    memset(&list, 0, sizeof(My402List));
    My402ListInit(&list);
        int num = ReadListFromFile(file, &list);
        sort(&list);
        PrintList(&list, num);
        if(file != stdin){
                fclose(file);
        }
    return 0;
}

