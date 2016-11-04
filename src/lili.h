/*
 * lili - Linked List Library
 * https://github.com/ricardocrudo/lili
 *
 * Copyright (c) 2016 Ricardo Crudo <ricardo.crudo@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LILI_H
#define LILI_H

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/


/*
****************************************************************************************************
*       MACROS
****************************************************************************************************
*/

// library version
#define LILI_VERSION    "1.0.0"

// macro to iterate all nodes of a list
#define LILI_FOREACH(list, var) \
    for(node_t *var = list->first; var; var = var->next)


/*
****************************************************************************************************
*       CONFIGURATION
****************************************************************************************************
*/

//#define LILI_ONLY_STATIC_ALLOCATION
//#define LILI_MAX_LISTS      10
//#define LILI_MAX_NODES      100


/*
****************************************************************************************************
*       DATA TYPES
****************************************************************************************************
*/

/**
 * @struct node_t
 * The node structure
 */
typedef struct node_t {
    struct node_t *prev;    //!< pointer to previous node
    struct node_t *next;    //!< pointer to next node
    void *data;             //!< pointer to node data
} node_t;

/**
 * @struct lili_t
 * The list structure
 */
typedef struct lili_t {
    int count;      //!< number of items in the list
    node_t *first;  //!< pointer to first node of the list
    node_t *last;   //!< pointer to last node of the list
} lili_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

/**
 * @defgroup lili_list List Functions
 * Set of functions to operate lists.
 * @{
 */

/**
 * Create a list
 *
 * @return pointer of a list object or NULL if memory allocation fail
 */
lili_t *lili_create(void);

/**
 * Destroy a list
 *
 * All items inside of the list will be destroyed.
 *
 * @param[in] list the list object
 */
void lili_destroy(lili_t *list);

/**
 * Push an item to list
 *
 * The item is pushed to the last position of the list.
 *
 * @param[in] list the list object
 * @param[in] data the data pointer to be stored
 */
void lili_push(lili_t *list, void *data);

/**
 * Pop an item from list
 *
 * The last item is taken from the list.
 *
 * @param[in] list the list object pointer
 *
 * @return the data pointer of the stored item
 */
void *lili_pop(lili_t *list);

/**
 * Push an item to beginning of the list
 *
 * The item is pushed to the first position of the list.
 *
 * @param[in] list the list object
 * @param[in] data the data pointer to be stored
 */
void lili_push_front(lili_t *list, void *data);

/**
 * Pop an item from the beginning of the list
 *
 * The first item is taken from the list.
 *
 * @param[in] list the list object pointer
 *
 * @return the data pointer of the stored item
 */
void *lili_pop_front(lili_t *list);

/**
 * Push an item to a specific position of the list
 *
 * The item is pushed to the position indicated by \a index,
 * where the first position is zero and the last is list->count.
 * The \a index might also have negative values, where -1 represents
 * the last position, -2 the next-to-last position and so on.
 *
 * @param[in] list the list object
 * @param[in] data the data pointer to be stored
 * @param[in] index the position where to push the item
 */
void lili_push_at(lili_t *list, void *data, int index);

/**
 * Pop an item from a specific position of the list
 *
 * The item is taken from the position indicated by \a index,
 * where the first position is zero and the last is list->count.
 * The \a index might also have negative values, where -1 represents
 * the last position, -2 the next-to-last position and so on.
 *
 * @param[in] list the list object
 * @param[in] index the position where to push the item
 *
 * @return the data pointer of the stored item
 */
void *lili_pop_from(lili_t *list, int index);

/**
 * @}
 */

/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/

#if defined(LILI_ONLY_STATIC_ALLOCATION) && \
  (!defined(LILI_MAX_LISTS) || !defined(LILI_MAX_NODES))
#error "LILI_ONLY_STATIC_ALLOCATION requires LILI_MAX_LISTS and LILI_MAX_NODES macros definition."
#endif


#endif
