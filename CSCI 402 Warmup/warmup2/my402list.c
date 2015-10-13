#include"cs402.h"
#include"my402list.h"
#include<stdlib.h>
int  My402ListLength(My402List* list)
{
        return list->num_members;
}

int  My402ListEmpty(My402List* list)
{
        return list->num_members == 0;
}

int  My402ListAppend(My402List* list, void* obj)
{
        My402ListElem* new_item = (My402ListElem* )malloc(sizeof(My402ListElem));
        if( new_item == NULL) return FALSE;
        new_item->obj = obj;
        new_item->next = &(list->anchor);
        new_item->prev = list->anchor.prev;
        list->anchor.prev->next = new_item;
        list->anchor.prev = new_item;
        ++(list->num_members);
        return TRUE;
}

int  My402ListPrepend(My402List* list, void* obj)
{
        My402ListElem* new_item = (My402ListElem* )malloc(sizeof(My402ListElem));
        if( new_item == NULL) return FALSE;
        new_item->obj = obj;
        new_item->next = list->anchor.next;
        new_item->prev = &(list->anchor);
        list->anchor.next->prev = new_item;
        list->anchor.next = new_item;
        ++(list->num_members);
        return TRUE;
}

void My402ListUnlink(My402List* list, My402ListElem* elem)
{
        My402ListElem* prev_elem = elem->prev;
        My402ListElem* next_elem = elem->next;
        // we do not have to worried about if the next or prev is null because of anchor pointer.
        prev_elem->next = next_elem;
        next_elem->prev = prev_elem;
        free(elem);
        --(list->num_members);
}

void My402ListUnlinkAll(My402List* list)
{
        My402ListElem* cur_elem = list->anchor.next;
        //free memory of all the elements
        while(cur_elem != &list->anchor){
                cur_elem = cur_elem->next;
                free(cur_elem->prev);
        }
        //reset the points in wsanchor
        list->anchor.next = &list->anchor;
        list->anchor.prev = &list->anchor;
        list->num_members = 0;
}

int  My402ListInsertAfter(My402List* list, void* obj, My402ListElem* elem)
{
        My402ListElem* new_item = (My402ListElem* )malloc(sizeof(My402ListElem));
        if( new_item == NULL) return FALSE;
        new_item->obj = obj;
        elem = (elem == NULL) ? &list->anchor : elem;
        new_item->next = elem->next;
        new_item->prev = elem;
        elem->next->prev = new_item;
        elem->next = new_item;
        ++(list->num_members);
        return TRUE;
}

int  My402ListInsertBefore(My402List* list, void* obj, My402ListElem* elem)
{
        My402ListElem* new_item = (My402ListElem* )malloc(sizeof(My402ListElem));
        if( new_item == NULL) return FALSE;
        new_item->obj = obj;
        elem = (elem == NULL) ? &list->anchor : elem;
        new_item->next = elem;
        new_item->prev = elem->prev;
        elem->prev->next = new_item;
        elem->prev = new_item;
        ++(list->num_members);
        return TRUE;
}

My402ListElem *My402ListFirst(My402List* list)
{
        return (list->num_members == 0) ? NULL : list->anchor.next;
}

My402ListElem *My402ListLast(My402List* list)
{
        return (list->num_members == 0) ? NULL : list->anchor.prev;
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem)
{
        return (elem->next == &list->anchor) ? NULL : elem->next;
}

My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem)
{
        return (elem->prev == &list->anchor) ? NULL : elem->prev;
}

My402ListElem *My402ListFind(My402List* list, void* obj)
{
        My402ListElem* cur_elem = list->anchor.next;
        while(cur_elem != &list->anchor){
                if(cur_elem->obj == obj){
                        return cur_elem;
                }
                cur_elem = cur_elem->next;
        }
        return NULL;
}

int My402ListInit(My402List* list)
{
        if(list == NULL) return FALSE;
        list->num_members = 0;
        list->anchor.next = &list->anchor;
        list->anchor.prev = &list->anchor;
        return TRUE;
}

