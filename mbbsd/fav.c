#include "bbs.h"

#ifdef MEM_CHECK
static int	memcheck;
#endif

/* the total number of items */
static int 	fav_number;

/* definition of fav stack, the top one is in use now. */
static int	fav_stack_num = 0;
static fav_t   *fav_stack[FAV_MAXDEPTH] = {0};

/* current fav_type_t placement */
static int	fav_place;

/* fav_tmp is for recordinge while copying, moving, etc. */
static fav_t   *fav_tmp;
//static int	fav_tmp_snum; /* the sequence number in favh in fav_t */


inline static void free_fp(fav_type_t *ft);

/* for casting */
inline static fav_board_t *cast_board(fav_type_t *p){
    return (fav_board_t *)p->fp;
}

inline static fav_line_t *cast_line(fav_type_t *p){
    return (fav_line_t *)p->fp;
}

inline static fav_folder_t *cast_folder(fav_type_t *p){
    return (fav_folder_t *)p->fp;
}
/* --- */

/* "current" means what at the position of the cursor */
inline fav_t *get_current_fav(void){
    if (fav_stack_num == 0)
	return NULL;
    return fav_stack[fav_stack_num - 1];
}

inline static fav_t *get_fav_folder(fav_type_t *ft){
    if (ft->type != FAVT_FOLDER)
	return NULL;
    return cast_folder(ft)->this_folder;
}

inline int get_item_type(fav_type_t *ft){
    return ft->type;
}

inline fav_type_t *get_current_entry(void){
    fav_t *fp = fav_stack[fav_stack_num - 1];
    if (fp->nDatas <= 0)
	return NULL;
    return &fp->favh[fav_place];
}

inline void fav_set_tmp_folder(fav_t *fp){
    fav_tmp = fp;
}

inline static fav_t *fav_get_tmp_fav(void){
    return fav_tmp;
}
/* --- */


inline static void line_decrease(fav_t *fp) {
//    fp->nLines--;  don't do it, we'll count when rebuild_fav
    fav_number--;
}

inline static void board_decrease(fav_t *fp) {
    fp->nBoards--;
    fav_number--;
}

inline static void folder_decrease(fav_t *fp) {
//    fp->nFolders--;  don't do it, we'll count when rebuild_fav
    fav_number--;
}
/* --- */

inline static void line_increase(fav_t *fp) {
    fp->nLines++;
    fav_number++;
}

inline static void board_increase(fav_t *fp) {
    fp->nBoards++;
    fav_number++;
}

inline static void folder_increase(fav_t *fp) {
    fp->nFolders++;
    fav_number++;
}
/* --- */

inline static int get_folder_num(fav_t *fp) {
    return fp->nFolders;
}

inline static int get_line_num(fav_t *fp) {
    return fp->nLines;
}

/* bool:
 *   0: unset 1: set 2: opposite */
inline void set_attr(fav_type_t *ft, int bit, int bool){
    if (bool == 2)
	bool = !(ft->attr & bit);
    if (bool)
	ft->attr |= bit;
    else
	ft->attr &= ~bit;
}

inline int is_set_attr(fav_type_t *ft, int bit){
    return ft->attr & bit;
}
/* --- */

static char *get_item_title(fav_type_t *ft)
{
    switch (get_item_type(ft)){
	case FAVT_BOARD:
	    return bcache[cast_board(ft)->bid - 1].brdname;
	case FAVT_FOLDER:
	    return cast_folder(ft)->title;
	case FAVT_LINE:
	    return "----";
    }
    return NULL;
}

static char *get_item_class(fav_type_t *ft)
{
    switch (get_item_type(ft)){
	case FAVT_BOARD:
	    return bcache[cast_board(ft)->bid - 1].title;
	case FAVT_FOLDER:
	    return "�ؿ�";
	case FAVT_LINE:
	    return "----";
    }
    return NULL;
}

#ifdef MEM_CHECK
inline void fav_set_memcheck(int n) {
    memcheck = n;
}

inline int fav_memcheck(void) {
    return memcheck;
}
#endif
/* ---*/

static int get_type_size(int type)
{
    switch (type){
	case FAVT_BOARD:
	    return sizeof(fav_board_t);
	case FAVT_FOLDER:
	    return sizeof(fav_folder_t);
	case FAVT_LINE:
	    return sizeof(fav_line_t);
    }
    return 0;
}

inline static void* fav_malloc(int size){
    void *p = (void *)malloc(size);
    memset(p, 0, size);
    return p;
}

/* allocate the header(fav_type_t) and item it self. */
static fav_type_t *fav_item_allocate(int type)
{
    int size = 0;
    fav_type_t *ft = (fav_type_t *)fav_malloc(sizeof(fav_type_t));

    size = get_type_size(type);
    if (size) {
	ft->fp = fav_malloc(size);
	ft->type = type;
    }
    return ft;
}

inline static fav_t *get_fav_root(void){
    return fav_stack[0];
}

/* return: the exact number after cleaning
 * reset the line number, board number, folder number, and total number (number)
 */
static void rebuild_fav(fav_t *fp)
{
    int i, bid;
    fav_number = 0;
    fp->nLines = fp->nBoards = fp->nFolders = 0;
    for (i = 0; i < fp->nAllocs; i++){
	if (!is_set_attr(&fp->favh[i], FAVH_FAV))
	    continue;
	switch (get_item_type(&fp->favh[i])){
	    case FAVT_BOARD:
		bid = cast_board(&fp->favh[i])->bid;
		if (!validboard(bid - 1))
		    continue;
		board_increase(fp);
		break;
	    case FAVT_LINE:
		line_increase(fp);
		break;
	    case FAVT_FOLDER:
		rebuild_fav(get_fav_folder(fp->favh[i].fp));
		folder_increase(fp);
		break;
	    default:
		continue;
	}
    }
}

inline void cleanup(void)
{
    rebuild_fav(get_fav_root());
}

/* sort the fav */
static int favcmp_by_name(const void *a, const void *b)
{
    return strcasecmp(get_item_title((fav_type_t *)a), get_item_title((fav_type_t *)b));
}

void fav_sort_by_name(void)
{
    rebuild_fav(get_current_fav());
    qsort(get_current_fav(), get_current_fav()->nDatas, sizeof(fav_type_t), favcmp_by_name);
}

static int favcmp_by_class(const void *a, const void *b)
{
    fav_type_t *f1, *f2;
    int cmp;

    f1 = (fav_type_t *)a;
    f2 = (fav_type_t *)b;
    if (f1->type == FAVT_FOLDER || f2->type == FAVT_FOLDER)
	return -1;
    if (f1->type == FAVT_LINE || f2->type == FAVT_LINE)
	return 1;

    cmp = strncasecmp(get_item_class(f1), get_item_class(f2), 4);
    if (cmp)
	return cmp;
    return strcasecmp(get_item_title(f1), get_item_title(f1));
}

void fav_sort_by_class(void)
{
    rebuild_fav(get_current_fav());
    qsort(get_current_fav(), get_current_fav()->nDatas, sizeof(fav_type_t), favcmp_by_class);
}
/* --- */

inline static void
fav_item_copy(fav_type_t *target, const fav_type_t *source){
    target->type = source->type;
    target->attr = source->attr;
    target->fp = source->fp;
}

/*
 * The following is the movement operations in the user interface.
 */
inline static int fav_stack_full(void){
    return fav_stack_num >= FAV_MAXDEPTH;
}

inline int fav_max_folder_level(void){
    return fav_stack_full();
}

inline static int fav_stack_push_fav(fav_t *fp){
    if (fav_stack_full())
	return -1;
    fav_stack[fav_stack_num++] = fp;
    fav_place = 0;
    return 0;
}

inline static int fav_stack_push(fav_type_t *ft){
//    if (ft->type != FAVT_FOLDER)
//	return -1;
    return fav_stack_push_fav(get_fav_folder(ft));
}

inline static void fav_stack_pop(void){
    fav_stack_num--;
    fav_place = 0;
}

void fav_folder_in(void)
{
    fav_type_t *tmp = get_current_entry();
    if (tmp->type == FAVT_FOLDER){
	fav_stack_push(tmp);
    }
}

void fav_folder_out(void)
{
    fav_stack_pop();
}

void fav_cursor_up(void)
{
    fav_t *ft = get_current_fav();
    do{
	if (fav_place == 0)
	    fav_place = ft->nDatas - 1;
	else
	    fav_place--;
    }while(!(ft->favh[fav_place].attr & FAVH_FAV));
}

void fav_cursor_down(void)
{
    fav_t *ft = get_current_fav();
    do{
	if (fav_place == ft->nDatas - 1)
	    fav_place = 0;
	else
	    fav_place++;
    }while(!(ft->favh[fav_place].attr & FAVH_FAV));
}

void fav_cursor_up_step(int step)
{
    int i;
    for(i = 0; i < step; i++){
	if (fav_place <= 0)
	    break;
	fav_cursor_up();
    }
}

void fav_cursor_down_step(int step)
{
    int i;
    for(i = 0; i < step; i++){
	if (fav_place >= get_current_fav()->nDatas)
	    break;
	fav_cursor_down();
    }
}

/* from up to down */
void fav_cursor_set(int where)
{
    fav_type_t *ft = get_current_entry();
    fav_place = 0;
    if (ft == NULL || ft->fp == NULL)
	return;
    while(!get_current_entry()->attr & FAVH_FAV)
	fav_cursor_down();
    fav_cursor_down_step(where);
}
/* --- */

/* load from the rec file */
static void read_favrec(int fd, fav_t *fp)
{
    int i, total;
    read(fd, &fp->nDatas, sizeof(fp->nDatas));
    read(fd, &fp->nBoards, sizeof(fp->nBoards));
    read(fd, &fp->nLines, sizeof(fp->nLines));
    read(fd, &fp->nFolders, sizeof(fp->nFolders));
    fp->nAllocs = fp->nDatas + FAV_PRE_ALLOC;
    total = fp->nBoards + fp->nLines + fp->nFolders;
    fp->favh = (fav_type_t *)fav_malloc(sizeof(fav_type_t) * total);

    for(i = 0; i < total; i++){
	read(fd, &fp->favh[i].type, sizeof(fp->favh[i].type));
	read(fd, &fp->favh[i].attr, sizeof(fp->favh[i].attr));
	fp->favh[i].fp = (void *)fav_malloc(get_type_size(fp->favh[i].type));
	read(fd, fp->favh[i].fp, get_type_size(fp->favh[i].type));
    }

    for(i = 0; i < total; i++){
	if (fp->favh[i].type == FAVT_FOLDER){
	    fav_t *p = get_fav_folder(&fp->favh[i]);
	    p = (fav_t *)fav_malloc(sizeof(fav_t));
	    read_favrec(fd, p);
	}
    }
}

int fav_load(void)
{
    int fd;
    char buf[128];
    fav_t *fp;
    if (fav_stack_num > 0)
	return -1;
    setuserfile(buf, FAV4);
    fd = open(buf, O_RDONLY);
    if (fd < 0){
	return -1;
    }
    fp = (fav_t *)fav_malloc(sizeof(fav_t));
    read_favrec(fd, fp);
    fav_stack_push_fav(fp);
    close(fd);
    fav_set_memcheck(MEM_CHECK);
    return 0;
}
/* --- */

/* write to the rec file */
static void write_favrec(int fd, fav_t *fp)
{
    int i, total;
    if (fp == NULL)
	return;
    write(fd, &fp->nDatas, sizeof(fp->nDatas));
    write(fd, &fp->nBoards, sizeof(fp->nBoards));
    write(fd, &fp->nLines, sizeof(fp->nLines));
    write(fd, &fp->nFolders, sizeof(fp->nFolders));
    total = fp->nBoards + fp->nLines + fp->nFolders;
    
    for(i = 0; i < total; i++){
	write(fd, &fp->favh[i].type, sizeof(fp->favh[i].type));
	write(fd, &fp->favh[i].attr, sizeof(fp->favh[i].attr));
	write(fd, fp->favh[i].fp, get_type_size(fp->favh[i].type));
    }
    
    for(i = 0; i < total; i++){
	if ((fp->favh[i].attr & FAVH_FAV) && (fp->favh[i].type == FAVT_FOLDER))
	    write_favrec(fd, get_fav_folder(&fp->favh[i]));
    }
}

int fav_save(void)
{
    int fd;
    char buf[128], buf2[128];
#ifdef MEM_CHECK
    if (fav_memcheck() != MEM_CHECK)
	return -1;
#endif
    fav_t *fp = get_fav_root();
    if (fp == NULL)
	return -1;
    cleanup();
    setuserfile(buf, FAV4".tmp");
    setuserfile(buf2, FAV4);
    fd = open(buf, O_TRUNC | O_WRONLY);
    if (fd < 0)
	return -1;
    write_favrec(fd, fp);
    close(fd);
    Rename(buf, buf2);
    return 0;
}
/* --- */

/* It didn't need to remove it self, just remove all the attributes.
 * It'll be remove when it save to the record file. */
static void fav_free_item(fav_type_t *ft)
{
    if (ft->fp)
	free_fp(ft->fp);
    set_attr(ft, 0xFFFF, FALSE);
//    ft = NULL;
}

static int fav_remove(fav_t *fp, fav_type_t *ft)
{
    switch(get_item_type(ft)){
	case FAVT_BOARD:
	    board_decrease(fp);
	    break;
	case FAVT_FOLDER:
	    folder_decrease(fp);
	    break;
	case FAVT_LINE:
	    line_decrease(fp);
	    break;
    }
    fav_free_item(ft);
    return 0;
}

/* free the mem of whole fav tree */
static void fav_free_branch(fav_t *fp)
{
    int i;
    fav_type_t *ft;
    if (fp == NULL)
	return;
    for(i = 0; i < fp->nAllocs; i++){
	ft = &fp->favh[i];
	if (ft->attr & FAVT_FOLDER)
	    fav_free_branch(get_fav_folder(ft));
	fav_remove(fp, ft);
    }
    free(fp);
    fp = NULL;
}

inline static void free_fp(fav_type_t *ft)
{
    if (ft->type == FAVT_FOLDER)
	fav_free_branch(get_fav_folder(ft));
    free(ft->fp);
    ft->fp = NULL;
}

void fav_free(void)
{
    fav_free_branch(get_fav_root());
}
/* --- */

void fav_remove_current(void)
{
    fav_remove(get_current_fav(), get_current_entry());
}

void fav_remove_board_from_whole(int bid)
{
    int i;
    fav_t *fp = get_current_fav();
    fav_type_t *ft;
    for(i = 0; i < fp->nAllocs; i++){
	ft = &fp->favh[i];
	if (ft->attr & FAVT_BOARD && cast_board(ft)->bid == bid)
	    fav_remove(fp, ft);
	else if (ft->attr & FAVT_FOLDER){
	    fav_stack_push(ft);
	    fav_remove_board_from_whole(bid);
	    fav_stack_pop();
	}
    }
}

static fav_type_t *get_fav_item(short id, int type)
{
    int i;
    fav_type_t *ft;
    fav_t *fp = get_current_fav();

    for(i = 0; i < fp->nAllocs; i++){
	ft = &fp->favh[i];
	if (!ft || get_item_type(ft) != type)
	    continue;
	if (fav_getid(ft) == id)
	    return ft;
    }
    return NULL;
}

static fav_type_t *getboard(short bid)
{
    return get_fav_item(bid, FAVT_BOARD);
}

char *get_folder_title(int fid)
{
    return get_item_title(get_fav_item(fid, FAVT_FOLDER));
}


char getbrdattr(short bid)
{
    fav_type_t *fb = getboard(bid);
    if (!fb)
	return 0;
    return fb->attr;
}

time_t getbrdtime(short bid)
{
    fav_type_t *fb = getboard(bid);
    if (!fb)
	return 0;
    return cast_board(fb)->lastvisit;
}

void setbrdtime(short bid, time_t t)
{
    fav_type_t *fb = getboard(bid);
    if (fb)
	cast_board(fb)->lastvisit = t;
}

int fav_getid(fav_type_t *ft)
{
    switch(get_item_type(ft)){
	case FAVT_FOLDER:
	    return cast_folder(ft)->fid;
	case FAVT_LINE:
	    return cast_line(ft)->lid;
	case FAVT_BOARD:
	    return cast_board(ft)->bid;
    }
    return -1;
}

/* suppose we don't add too much fav_type_t at the same time. */
static int enlarge_if_full(fav_t *fp)
{
    /* enlarge the volume if need. */
    if (fp->nDatas >= MAX_FAV)
	return -1;
    if (fp->nDatas != fp->nAllocs)
	return 1;

    /* realloc and clean the tail */
    fp->favh = (fav_type_t *)realloc(fp->favh, sizeof(fav_type_t) * (fp->nAllocs + FAV_PRE_ALLOC));
    memset(fp->favh + fp->nAllocs, 0, FAV_PRE_ALLOC);
    fp->nAllocs += FAV_PRE_ALLOC;
    return 0;
}

inline int is_maxsize(void){
    return fav_number >= MAX_FAV;
}

int fav_add(fav_t *fp, fav_type_t *item)
{
    if (enlarge_if_full(fp) < 0)
	return -1;
    fav_item_copy(&fp->favh[fp->nDatas], item);
    fp->nDatas++;
    return 0;
}

/*
static void fav_move_into_folder(fav_t *from, int where, fav_t *to)
{
    fav_type_t *tmp = &from->favh[where];
    if (from != to){
	if (enlarge_if_full(to) < 0)
	    return;
    }
    fav_add(to, tmp);
    fav_remove(from, tmp);
}
*/

/* just move, in one folder */
static void move_in_folder(fav_t *fav, int from, int to)
{
    int i;
    fav_type_t tmp;
    fav_item_copy(&tmp, &fav->favh[from]);

    if (from < to) {
	for(i = from; i < to; i++)
	    fav_item_copy(&fav->favh[i], &fav->favh[i + 1]);
    }
    else { // to < from
	for(i = from; i > to; i--)
	    fav_item_copy(&fav->favh[i], &fav->favh[i - 1]);
    }
    fav_item_copy(&fav->favh[to], &tmp);
}

void move_in_current_folder(int from, int to)
{
    move_in_folder(get_current_fav(), from, to);
}

/* the following defines the interface of add new fav_XXX */
inline static fav_t *alloc_fav_item(void){
    fav_t *fp = (fav_t *)fav_malloc(sizeof(fav_t));
    fp->favh = (fav_type_t *)fav_malloc(sizeof(fav_type_t) * FAV_PRE_ALLOC);
    return fp;
}

static fav_type_t *init_add(fav_t *fp, int type, int place)
{
    fav_type_t *ft;
    if (is_maxsize())
	return NULL;
    ft = fav_item_allocate(type);
    set_attr(ft, FAVH_FAV, TRUE);
    fav_add(fp, ft);
    if (place >= 0 && place < fp->nAllocs)
	move_in_folder(fp, fp->nDatas - 1, place);
    return ft;
}

/* if place < 0, just put the item to the tail */
fav_type_t *fav_add_line(int place)
{
    fav_t *fp = get_current_fav();
    fav_type_t *ft = init_add(fp, FAVT_LINE, place);
    if (ft == NULL)
	return NULL;
    line_increase(fp);
    cast_line(ft)->lid = get_line_num(fp);
    return ft;
}

fav_type_t *fav_add_folder(int place)
{
    fav_t *fp = get_current_fav();
    fav_type_t *ft;
    if (fav_stack_full())
	return NULL;
    ft = init_add(fp, FAVT_FOLDER, place);
    if (ft == NULL)
	return NULL;
    cast_folder(ft)->this_folder = alloc_fav_item();
    folder_increase(fp);
    cast_folder(ft)->fid = get_folder_num(fp); // after folder_increase
    return ft;
}

fav_type_t *fav_add_board(int bid, int place)
{
    fav_t *fp = get_current_fav();
    fav_type_t *ft = init_add(fp, FAVT_BOARD, place);
    if (ft == NULL)
	return NULL;
    cast_board(ft)->bid = bid;
    board_increase(fp);
    return ft;
}
/* --- */

/* everything about the tag in fav mode.
 * I think we don't have to implement the function 'cross-folder' tag.*/
inline void fav_tag_current(int bool) {
    set_attr(get_current_entry(), FAVH_TAG, bool);
}

static void fav_dosomething_tagged_item(fav_t *fp, int (*act)(fav_t *, fav_type_t *))
{
    int i;
    for(i = 0; i < fp->nAllocs; i++){
	if (is_set_attr(&fp->favh[i], FAVH_TAG))
	    (*act)(fp, &fp->favh[i]);
    }
}

inline static void fav_remove_tagged_item(fav_t *fp){
    fav_dosomething_tagged_item(fp, fav_remove);
}

static int add_and_remove_tag(fav_t *fp, fav_type_t *ft)
{
    fav_type_t *tmp = fav_malloc(sizeof(fav_type_t));
    fav_item_copy(tmp, ft);
    if (fav_add(fav_get_tmp_fav(), tmp) < 0)
	return -1;
    fav_remove(fp, ft);
    set_attr(tmp, FAVH_TAG, FALSE);
    return 0;
}

inline static void fav_add_tagged_item(fav_t *fp){
    fav_dosomething_tagged_item(fp, add_and_remove_tag);
}

static void fav_do_recursively(void (*act)(fav_t *))
{
    int i;
    fav_type_t *ft;
    fav_t *fp = get_current_fav();
    for(i = 0; i < fp->nAllocs; i++){
	ft = &fp->favh[i];
	if (get_item_type(ft) == FAVT_FOLDER){
	    fav_stack_push(ft);
	    fav_do_recursively(act);
	    fav_stack_pop();
	}
    }
    (*act)(fp);
}

static void fav_dosomething_all_tagged_item(void (*act)(fav_t *))
{
    int tmp = fav_stack_num;
    fav_stack_num = 1;
    fav_do_recursively(act);
    fav_stack_num = tmp;
}

void fav_remove_all_tagged_item(void)
{
    fav_dosomething_all_tagged_item(fav_remove_tagged_item);
}

void fav_add_all_tagged_item(void)
{
    fav_set_tmp_folder(get_current_fav());
    fav_dosomething_all_tagged_item(fav_add_tagged_item);
}

inline static int remove_tag(fav_t *fp, fav_type_t  *ft)
{
    set_attr(ft, FAVH_TAG, FALSE);
    return 0;
}

inline static void remove_tags(fav_t *fp)
{
    fav_dosomething_tagged_item(fp, remove_tag);
}

void fav_remove_all_tag(void)
{
    fav_dosomething_all_tagged_item(remove_tags);
}
/* --- */

void fav_set_folder_title(fav_type_t *ft, char *title)
{
    if (ft->type != FAVT_FOLDER)
	return;
    strlcpy(cast_folder(ft)->title, title, sizeof(cast_folder(ft)->title));
}

/* old struct */
#define BRD_UNREAD      1
#define BRD_FAV         2
#define BRD_LINE        4
#define BRD_TAG         8 
#define BRD_GRP_HEADER 16

typedef struct {
  short           bid;
  char            attr;
  time_t          lastvisit;
} fav3_board_t;

typedef struct {
    short           nDatas;
    short           nAllocs;
    char            nLines;
    fav_board_t    *b;
} fav3_t;

int fav_v3_to_v4(void)
{
    int i, fd, fdw;
    char buf[128];

    short nDatas;
    char nLines;
    fav_t fav4;
    fav3_board_t *brd;
    
    setuserfile(buf, FAV4);
    fd = open(buf, O_RDONLY);
    if (fd >= 0){
	close(fd);
	return 0;
    }
    fdw = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fdw < 0)
	return -1;
    setuserfile(buf, FAV3);
    fd = open(buf, O_RDONLY);
    if (fd < 0)
	return -1;

    read(fd, &nDatas, sizeof(nDatas));
    read(fd, &nLines, sizeof(nLines));

    fav4.nDatas = nDatas;
    fav4.nBoards = nDatas - (-nLines);
    fav4.nLines = -nLines;
    fav4.nFolders = 0;
    fav4.favh = (fav_type_t *)fav_malloc(sizeof(fav_type_t) * fav4.nDatas);

    brd = (fav3_board_t *)fav_malloc(sizeof(fav3_board_t) * nDatas);
    read(fd, brd, sizeof(fav3_board_t) * nDatas);

    for(i = 0; i < fav4.nDatas; i++){
	fav4.favh[i].type = brd[i].attr & BRD_LINE ? FAVT_LINE : FAVT_BOARD;

	if (brd[i].attr & BRD_UNREAD)
	    fav4.favh[i].attr |= FAVH_UNREAD;
	if (brd[i].attr & BRD_FAV)
	    fav4.favh[i].attr |= FAVH_FAV;
	if (brd[i].attr & BRD_TAG)
	    fav4.favh[i].attr |= FAVH_TAG;

	fav4.favh[i].fp = (void *)fav_malloc(get_type_size(fav4.favh[i].type));
	if (brd[i].attr & BRD_LINE){
	    fav4.favh[i].type = FAVT_LINE;
	    cast_line(&fav4.favh[i])->lid = -brd[i].bid;
	}
	else{
	    fav4.favh[i].type = FAVT_BOARD;
	    cast_board(&fav4.favh[i])->bid = brd[i].bid;
	    cast_board(&fav4.favh[i])->lastvisit = brd[i].lastvisit;
	}
    }

    write_favrec(fdw, &fav4);
    fav_free_branch(&fav4);
    free(brd);
    return 0;
}
