#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/*
 * 이중 연결 리스트
 * 이중 연결 리스트의 구현은 동적으로 할당된 메모리를 필요로 하지 않는다.
 * 대신, 잠재적인 리스트 요소의 각 구조체는 struct list_elem을 포함해야 한다.
 * 모든 리스트 함수는 이러한 'struct list_elem'에 대해 작동한다.
 * list_entry 매크로는 struct list_elem에서 포함하는 구조체 객체로의 변환을 할 수 있다.
 * 즉, a를 b로 전환할 수 있는데, 여기서 a는 struct list_elem이고, b는 struct list_elem을 포함하는 구조체이다.
 *
 * 예를 들어, 'struct foo'의 리스트가 필요한 경우 'struct foo'는 다음과 같이
 * 'struct list_elem' 멤버를 포함해야 한다.
 *
 * struct foo {
 *   struct list_elem elem;
 *   int bar;
 *   ...other members...
 * };
 *
 * 그럼 'struct foo'의 리스트는 다음과 같이 선언 및 초기화할 수 있다.
 *
 * struct list foo_list;
 *
 * list_init(&foo_list);
 *
 * 반복은 struct list_elem에서 포함하는 구조체로의 변환이 필요한 전형적인 상황이다.
 * 다음은 foo_list를 사용한 예이다.
 *
 * struct list_elem *e;
 *
 * for (e = list_begin(&foo_list); e != list_end(&foo_list); e = list_next(e)) {
 *   struct foo *f = list_entry(e, struct foo, elem);
 *   ...f로 무언가를 수행...
 * }
 *
 * 소스 곳곳에서 리스트 사용에 대한 실제 예시를 찾을 수 있다.
 * 예를 들어, threads 디렉토리의 malloc.c, palloc.c, thread.c는 모두 리스트를 사용한다.
 *
 * 이 list 인터페이스는 C++ STL의 list<> template에서 영감을 얻었다.
 * 하지만 이 list는 타입 검사를 '전혀' 수행하지 않으며 다른 많은 정확성 검사를 수행할 수 없다.
 * 실수하면 큰일 날 수 있다.
 *
 * list 용어 목록:
 * - "front" : 목록의 첫 번째 요소입니다. 빈 리스트에서는 정의되지 않습니다. list_front()에 의해 반환됩니다.
 *
 * - "back" : 목록의 마지막 요소. 빈 리스트에서 정의되지 않았습니다.  list_back()에 의해 반환됩니다.
 *
 * - "tail" : 비유적으로 목록의 마지막 요소 바로 뒤에 있는 요소입니다. 빈 목록에서도 잘 정의됩니다.
 * list_end()에 의해 반환됩니다. 앞쪽에서 뒤쪽으로의 반복을 위한 마지막 센티널로 사용됩니다.
 *
 * - "beginning" : 비어 있지 않은 목록에서는 앞부분입니다. 비어 있는 리스트에서는 꼬리입니다.
 * list_begin()에 의해 반환됩니다. 앞쪽에서 뒤쪽으로의 반복의 시작점으로 사용됩니다.
 *
 * - "head": 비유적으로 목록의 첫 번째 요소 바로 앞에 있는 요소입니다. 빈 목록에서도 잘 정의됩니다.
 * list_rend()에 의해 반환됩니다.  뒤에서 앞으로의 반복을 위한 마지막 센티널로 사용됩니다.
 *
 * - "reverse beginning": 비어 있지 않은 목록에서는 뒤쪽입니다. 비어 있는 리스트에서는 머리입니다.
 * list_rbegin()에 의해 반환됩니다. 뒤에서 앞으로의 반복을 위한 시작점으로 사용됩니다.
 *
 * - "interior element": 머리 또는 꼬리가 아닌 요소, 즉 실제 목록 요소입니다. 빈 목록에는 내부 요소가 없습니다.
 * 
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* List element. */
struct list_elem
{
   struct list_elem *prev; /* Previous list element. */
   struct list_elem *next; /* Next list element. */
};

/* List. */
struct list
{
   struct list_elem head; /* List head. */
   struct list_elem tail; /* List tail. */
};

/*
 * list_entry MACRO : list element LIST_ELEM 포인터를 LIST_ELEM이 포함된 구조체에 대한 포인터로 변환한다.
 * STRUCT는 외부 구조체의 이름이고, MEMBER는 리스트 요소의 멤버 이름이다.
 * 파일 상단의 큰 주석을 참조하면 예제를 볼 수 있다.
 */
#define list_entry(LIST_ELEM, STRUCT, MEMBER) \
   ((STRUCT *)((uint8_t *)&(LIST_ELEM)->next - offsetof(STRUCT, MEMBER.next)))

void list_init(struct list *);

/* List traversal. */
struct list_elem *list_begin(struct list *);
struct list_elem *list_next(struct list_elem *);
struct list_elem *list_end(struct list *);

struct list_elem *list_rbegin(struct list *);
struct list_elem *list_prev(struct list_elem *);
struct list_elem *list_rend(struct list *);

struct list_elem *list_head(struct list *);
struct list_elem *list_tail(struct list *);

/* List insertion. */
void list_insert(struct list_elem *, struct list_elem *);
void list_splice(struct list_elem *before,
                 struct list_elem *first, struct list_elem *last);
void list_push_front(struct list *, struct list_elem *);
void list_push_back(struct list *, struct list_elem *);

/* List removal. */
struct list_elem *list_remove(struct list_elem *);
struct list_elem *list_pop_front(struct list *);
struct list_elem *list_pop_back(struct list *);

/* List elements. */
struct list_elem *list_front(struct list *);
struct list_elem *list_back(struct list *);

/* List properties. */
size_t list_size(struct list *);
bool list_empty(struct list *);

/* Miscellaneous. */
void list_reverse(struct list *);

/* 보조 데이터 AUX가 주어진 두 리스트 요소 A와 B의 값을 비교합니다.
 * A가 B보다 작으면 true를 반환하고, A가 B보다 크거나 같으면 false를 반환합니다.
 */
typedef bool list_less_func(const struct list_elem *a,
                            const struct list_elem *b,
                            void *aux);

/* Operations on lists with ordered elements. */
void list_sort(struct list *,
               list_less_func *, void *aux);
void list_insert_ordered(struct list *, struct list_elem *,
                         list_less_func *, void *aux);
void list_unique(struct list *, struct list *duplicates,
                 list_less_func *, void *aux);

/* Max and min. */
struct list_elem *list_max(struct list *, list_less_func *, void *aux);
struct list_elem *list_min(struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
