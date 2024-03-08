#include "list.h"
#include "../debug.h"

/* 우리의 이중 연결 리스트에는 두 개의 헤더 요소가 있습니다:
 * 첫 번째 요소 바로 앞에 있는 "head"와 마지막 요소 바로 뒤에 있는 "tail"입니다.
 * 앞 헤더의 `prev` 링크와 뒤 헤더의 `next` 링크는 null입니다.
 * 그들의 다른 두 링크는 리스트의 내부 요소를 통해 서로를 가리킵니다.

   An empty list looks like this:

   			+------+     +------+
   null <---| head |<--->| tail |---> null
   			+------+     +------+

   A list with two elements in it looks like this:

 			+------+     +-------+     +-------+     +------+
   null <---| head |<--->|   1   |<--->|   2   |<--->| tail |<---> null
   			+------+     +-------+     +-------+     +------+

   The symmetry of this arrangement eliminates lots of special
   cases in list processing.  For example, take a look at
   list_remove(): it takes only two pointer assignments and no
   conditionals.  That's a lot simpler than the code would be
   without header elements.

   (Because only one of the pointers in each header element is used,
   we could in fact combine them into a single header element
   without sacrificing this simplicity.  But using two separate
   elements allows us to do a little bit of checking on some
   operations, which can be valuable.)
*/

/*사용하지 않음*/
static bool is_sorted(struct list_elem *a, struct list_elem *b,
 list_less_func *less, void *aux) UNUSED;

/* 
* Returns true if ELEM is a head, false otherwise. 
* elem이 head면 true를 반환하고, 그렇지 않으면 false를 반환합니다.	
*/
static inline bool
is_head(struct list_elem *elem)
{
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* Returns true if ELEM is an interior element, false otherwise. 
* elem이 interior이면 true를 반환하고, 그렇지 않으면 false를 반환합니다.
* head와 tail 사이에 있는 경우
*/
static inline bool
is_interior(struct list_elem *elem)
{
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* Returns true if ELEM is a tail, false otherwise. 
* elem이 tail이면 true를 반환하고, 그렇지 않으면 false를 반환합니다.
* elem이 null이 아니고 앞은 존재하는데 다음이 NULL 인경우 tail
*/
static inline bool
is_tail(struct list_elem *elem)
{
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* Initializes LIST as an empty list. 
* LIST를 빈 리스트로 초기화합니다.
* 리스트의 head와 tail을 초기화합니다.
* head의 prev는 NULL, next는 tail을 가리키고
* tail의 prev는 head를 가리키고 next는 NULL을 가리킵니다.
* 리스트의 head와 tail은 리스트의 시작과 끝을 나타냅니다.
* 리스트의 시작과 끝에는 head와 tail이 있습니다.
* head는 리스트의 첫 번째 요소 앞에 있고, tail은 마지막 요소 뒤에 있습니다.
* head의 prev 링크와 tail의 next 링크는 null입니다.	
*/
void list_init(struct list *list)
{
	ASSERT(list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* Returns the beginning of LIST.  
* LIST의 시작을 반환합니다.
* 리스트가 가르키고 있는 헤드의 다음 요소 즉 첫번째 요소를 반환한다.
*/
struct list_elem *
list_begin(struct list *list)
{
	ASSERT(list != NULL);
	return list->head.next;
}

/* Returns the element after ELEM in its list.  If ELEM is the
*  last element in its list, returns the list tail.  Results are
*  undefined if ELEM is itself a list tail. 
* elem의 다음 요소를 반환합니다. elem이 리스트의 마지막 요소인 경우 리스트의 tail을 반환합니다. 
* elem이 자체적으로 리스트 tail인 경우 결과는 정의되지 않습니다.   
* elem이 head이거나 interior인 경우 elem의 next를 반환합니다.
* elem이 NULL이 아거나 tail인 경우 NULL을 반환합니다.
* elem이 NULL이 아거나 head인 경우 elem의 next를 반환합니다.   
*/
struct list_elem *
list_next(struct list_elem *elem)
{
	ASSERT(is_head(elem) || is_interior(elem));
	return elem->next;
}

/* Returns LIST's tail.
*   list_end() is often used in iterating through a list from
*   front to back.  See the big comment at the top of list.h for
*   an example. 
* 리스트의 tail을 반환합니다.
*   
*/
struct list_elem *
list_end(struct list *list)
{
	ASSERT(list != NULL);
	return &list->tail;
}

/* Returns the LIST's reverse beginning, for iterating through
   LIST in reverse order, from back to front. 
* tail이 아닌 맨 마지막 요소를 반환한다.
*/
struct list_elem *
list_rbegin(struct list *list)
{
	ASSERT(list != NULL);
	return list->tail.prev;
}

/* Returns the element before ELEM in its list.  If ELEM is the
   first element in its list, returns the list head.  Results are
   undefined if ELEM is itself a list head. 
* elem의 이전 요소를 반환합니다. elem이 리스트의 첫 번째 요소인 경우 리스트의 head를 반환합니다.
* elem이 head이거나 interior인 경우 elem의 prev를 반환합니다.
* elem이 NULL이 아거나 head인 경우 NULL을 반환합니다.
*   
   */
struct list_elem *
list_prev(struct list_elem *elem)
{
	ASSERT(is_interior(elem) || is_tail(elem));
	return elem->prev;
}

/* Returns LIST's head.

   list_rend() is often used in iterating through a list in
   reverse order, from back to front.  Here's typical usage,
   following the example from the top of list.h:

	list_rend()는 목록을 뒤에서 앞으로 역순으로 반복하는 데 자주 사용됩니다.  
	다음은 list.h의 맨 위에 있는 예제에 따른 일반적인 사용법입니다:
   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list); e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...do something with f...
   }
   리스트의 헤드를 반환
   */
struct list_elem *
list_rend(struct list *list)
{
	ASSERT(list != NULL);
	return &list->head;
}

/* Return's LIST's head.

   list_head() can be used for an alternate style of iterating
   through a list, e.g.:

   e = list_head (&list);
   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   리스트의 헤드를 반환
   */
struct list_elem *
list_head(struct list *list)
{
	ASSERT(list != NULL);
	return &list->head;
}

/* Return's LIST's tail. 
* 리스트의 꼬리를 반환
*/
struct list_elem *
list_tail(struct list *list)
{
	ASSERT(list != NULL);
	return &list->tail;
}

/* Inserts ELEM just before BEFORE, which may be either an
   interior element or a tail.  The latter case is equivalent to
   list_push_back(). 
* Before 바로 앞에 ELEM을 삽입
* 꼬리인경우 list_push_back()과 동일
*/
void list_insert(struct list_elem *before, struct list_elem *elem)
{
	ASSERT(is_interior(before) || is_tail(before));
	ASSERT(elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* Removes elements FIRST though LAST (exclusive) from their
*   current list, then inserts them just before BEFORE, which may
*   be either an interior element or a tail. 
*    last -> 2, 3, 4, 5에서 2, 3, 4를 옮기는 경우 last는 5
*    before -> 5 뒤로 옮기는 경우 before는 5
*    first -> 2, 3, 4 를 옮기는 경우 first는 2
*   일부 구간을 잘라서 어느 지점 뒤로 옮긴다.
*/
void list_splice(struct list_elem *before,
				 struct list_elem *first, struct list_elem *last)
{
	ASSERT(is_interior(before) || is_tail(before));
	if (first == last)
		return;
	last = list_prev(last);

	ASSERT(is_interior(first));
	ASSERT(is_interior(last));

	/* Cleanly remove FIRST...LAST from its current list. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* Splice FIRST...LAST into new list. */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* Inserts ELEM at the beginning of LIST, so that it becomes the
   front in LIST. 
   * begin은 head의 다음 요소를 반환한다.
   * head 다음 요소 앞에 elem을 삽입
   * 결론 맨 앞에 elem을 삽입
   */
void list_push_front(struct list *list, struct list_elem *elem)
{
	list_insert(list_begin(list), elem);
}

/* Inserts ELEM at the end of LIST, so that it becomes the
   back in LIST. 
* list_end -> 리스트의 꼬리를 반환
* 꼬리 앞에 elem을 삽입
* 리스트 맨 뒤에 elem을 삽입
*/
void list_push_back(struct list *list, struct list_elem *elem)
{
	list_insert(list_end(list), elem);
}

/* Removes ELEM from its list and returns the element that
   followed it.  Undefined behavior if ELEM is not in a list.

   It's not safe to treat ELEM as an element in a list after
   removing it.  In particular, using list_next() or list_prev()
   on ELEM after removal yields undefined behavior.  This means
   that a naive loop to remove the elements in a list will fail:

 ** DON'T DO THIS **
 	** 잘못 된 예시 **
 	for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 	{
 		...do something with e...
 		list_remove (e);
 	}
 ** DON'T DO THIS **

 Here is one correct way to iterate and remove elements from a
list:
** 올바른 제거방법 1 **
이 방법은 list_remove 함수가 제거된 요소 다음에 오는 요소를 반환하기 때문에 안전합니다. 
따라서, e를 제거한 후 다음 요소로의 이동이 자연스럽게 이루어집니다.
for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

If you need to free() elements of the list then you need to be
more conservative.  Here's an alternate strategy that works
even in that case:
** 올바른 제거방법 2 **
 리스트가 비어있지 않은 동안 리스트의 맨 앞에서 요소를 제거(list_pop_front)하고, 
 그 요소를 처리합니다. list_pop_front 함수는 리스트의 첫 번째 요소를 제거하고 그 요소를 반환합니다. 
 이 방법은 요소를 free() 하거나 메모리를 관리해야 할 때 특히 유용
while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
* 삭제된 다음 요소를 반환
*/
struct list_elem *
list_remove(struct list_elem *elem)
{
	ASSERT(is_interior(elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* Removes the front element from LIST and returns it.
   Undefined behavior if LIST is empty before removal. 
*  list_front는 헤드 다음 요소를 반환
*  *front는 맨 앞 요소의 주소  
* 맨 앞 요소를 삭제하고 반환
*/
struct list_elem *
list_pop_front(struct list *list)
{
	struct list_elem *front = list_front(list);
	list_remove(front);
	return front;
}

/* Removes the back element from LIST and returns it.
   Undefined behavior if LIST is empty before removal.
*  list_back은 tail 앞 요소인 맨 뒤 요소를 반환
*  *back는 맨 뒤 요소의 주소
*  맨 뒤 요소를 삭제하고 반환
*/
struct list_elem *
list_pop_back(struct list *list)
{
	struct list_elem *back = list_back(list);
	list_remove(back);
	return back;
}

/* Returns the front element in LIST.
   Undefined behavior if LIST is empty. 
* 리스트의 맨 앞 요소를 반환
*/
struct list_elem *
list_front(struct list *list)
{
	ASSERT(!list_empty(list));
	return list->head.next;
}

/* Returns the back element in LIST.
   Undefined behavior if LIST is empty. 
 * 리스트 맨뒤 요소를 반환
 * 꼬리 앞 값  
*/
struct list_elem *
list_back(struct list *list)
{
	ASSERT(!list_empty(list));
	return list->tail.prev;
}

/* Returns the number of elements in LIST.
   Runs in O(n) in the number of elements. 
* 리스트의 요소 수를 반환 리스트 내 개수 반환
*/
size_t
list_size(struct list *list)
{
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin(list); e != list_end(list); e = list_next(e))
		cnt++;
	return cnt;
}

/* Returns true if LIST is empty, false otherwise.
* 리스트가 비어있는지 확인 
* head의 다음요소가 tail이면 비어있는 리스트 
*/
bool list_empty(struct list *list)
{
	return list_begin(list) == list_end(list);
}

/* Swaps the `struct list_elem *'s that A and B point to. 
* elem a와 b를 바꿈
*/
static void
swap(struct list_elem **a, struct list_elem **b)
{
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* Reverses the order of LIST. 
* 리스트의 순서를 뒤집음
* 확인은 안해봄
*/
void list_reverse(struct list *list)
{
	if (!list_empty(list))
	{
		struct list_elem *e;

		for (e = list_begin(list); e != list_end(list); e = e->prev)
			swap(&e->prev, &e->next);
		swap(&list->head.next, &list->tail.prev);
		swap(&list->head.next->prev, &list->tail.prev->next);
	}
}

/* Returns true only if the list elements A through B (exclusive)
   are in order according to LESS given auxiliary data AUX. */
static bool
is_sorted(struct list_elem *a, struct list_elem *b,
		  list_less_func *less, void *aux)
{
	if (a != b)
		while ((a = list_next(a)) != b)
			if (less(a, list_prev(a), aux))
				return false;
	return true;
}

/* Finds a run, starting at A and ending not after B, of list
   elements that are in nondecreasing order according to LESS
   given auxiliary data AUX.  Returns the (exclusive) end of the
   run.
   A through B (exclusive) must form a non-empty range. */
static struct list_elem *
find_end_of_run(struct list_elem *a, struct list_elem *b,
				list_less_func *less, void *aux)
{
	ASSERT(a != NULL);
	ASSERT(b != NULL);
	ASSERT(less != NULL);
	ASSERT(a != b);

	do
	{
		a = list_next(a);
	} while (a != b && !less(a, list_prev(a), aux));
	return a;
}

/* Merges A0 through A1B0 (exclusive) with A1B0 through B1
   (exclusive) to form a combined range also ending at B1
   (exclusive).  Both input ranges must be nonempty and sorted in
   nondecreasing order according to LESS given auxiliary data
   AUX.  The output range will be sorted the same way. */
static void
inplace_merge(struct list_elem *a0, struct list_elem *a1b0,
			  struct list_elem *b1,
			  list_less_func *less, void *aux)
{
	ASSERT(a0 != NULL);
	ASSERT(a1b0 != NULL);
	ASSERT(b1 != NULL);
	ASSERT(less != NULL);
	ASSERT(is_sorted(a0, a1b0, less, aux));
	ASSERT(is_sorted(a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less(a1b0, a0, aux))
			a0 = list_next(a0);
		else
		{
			a1b0 = list_next(a1b0);
			list_splice(a0, list_prev(a1b0), a1b0);
		}
}

/* Sorts LIST according to LESS given auxiliary data AUX, using a
   natural iterative merge sort that runs in O(n lg n) time and
   O(1) space in the number of elements in LIST. */
void list_sort(struct list *list, list_less_func *less, void *aux)
{
	size_t output_run_cnt; /* Number of runs output in current pass. */

	ASSERT(list != NULL);
	ASSERT(less != NULL);

	/* Pass over the list repeatedly, merging adjacent runs of
	   nondecreasing elements, until only one run is left. */
	do
	{
		struct list_elem *a0;	/* Start of first run. */
		struct list_elem *a1b0; /* End of first run, start of second. */
		struct list_elem *b1;	/* End of second run. */

		output_run_cnt = 0;
		for (a0 = list_begin(list); a0 != list_end(list); a0 = b1)
		{
			/* Each iteration produces one output run. */
			output_run_cnt++;

			/* Locate two adjacent runs of nondecreasing elements
			   A0...A1B0 and A1B0...B1. */
			a1b0 = find_end_of_run(a0, list_end(list), less, aux);
			if (a1b0 == list_end(list))
				break;
			b1 = find_end_of_run(a1b0, list_end(list), less, aux);

			/* Merge the runs. */
			inplace_merge(a0, a1b0, b1, less, aux);
		}
	} while (output_run_cnt > 1);

	ASSERT(is_sorted(list_begin(list), list_end(list), less, aux));
}

/* Inserts ELEM in the proper position in LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST. */
void list_insert_ordered(struct list *list, struct list_elem *elem,
						 list_less_func *less, void *aux)
{
	struct list_elem *e;

	ASSERT(list != NULL);
	ASSERT(elem != NULL);
	ASSERT(less != NULL);

	for (e = list_begin(list); e != list_end(list); e = list_next(e))
		if (less(elem, e, aux))
			break;
	return list_insert(e, elem);
}

/* Iterates through LIST and removes all but the first in each
   set of adjacent elements that are equal according to LESS
   given auxiliary data AUX.  If DUPLICATES is non-null, then the
   elements from LIST are appended to DUPLICATES. */
void list_unique(struct list *list, struct list *duplicates,
				 list_less_func *less, void *aux)
{
	struct list_elem *elem, *next;

	ASSERT(list != NULL);
	ASSERT(less != NULL);
	if (list_empty(list))
		return;

	elem = list_begin(list);
	while ((next = list_next(elem)) != list_end(list))
		if (!less(elem, next, aux) && !less(next, elem, aux))
		{
			list_remove(next);
			if (duplicates != NULL)
				list_push_back(duplicates, next);
		}
		else
			elem = next;
}

/* Returns the element in LIST with the largest value according
   to LESS given auxiliary data AUX.  If there is more than one
   maximum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail. */
struct list_elem *
list_max(struct list *list, list_less_func *less, void *aux)
{
	struct list_elem *max = list_begin(list);
	if (max != list_end(list))
	{
		struct list_elem *e;

		for (e = list_next(max); e != list_end(list); e = list_next(e))
			if (less(max, e, aux))
				max = e;
	}
	return max;
}

/* Returns the element in LIST with the smallest value according
   to LESS given auxiliary data AUX.  If there is more than one
   minimum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail. */
struct list_elem *
list_min(struct list *list, list_less_func *less, void *aux)
{
	struct list_elem *min = list_begin(list);
	if (min != list_end(list))
	{
		struct list_elem *e;

		for (e = list_next(min); e != list_end(list); e = list_next(e))
			if (less(e, min, aux))
				min = e;
	}
	return min;
}
