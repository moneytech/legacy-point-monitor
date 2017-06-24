/*
 * Description:
 *   Provide support data structures to sub-projects
 *
 * void install_point(void* addr, int index, Point* point)
 * 	attempts to install a copy of the given point at the memory address given.
 *
 * void invalidate_point(void* addr, int index)
 * 	attempts to set the is_valid flag at the pointer-arithmatic index offset from
 * 	the given address.
 *
 * void show_task(void *task)
 * 	given a task struct, dump a representation to the log
 *
 * void show_points(void* shmaddr, int max)
 * 	given a pointer to an array of point structs, dump a representation of all
 * 	found points to the log. This is restricted up to (shmaddr + max) address
 * 	(note: in pointer arithmatic, not bytes).
 */

#define MAX_NUM_POINTS 20

typedef struct Point {
	int is_valid;
	float x;
	float y;
} Point;

typedef struct PointTask {
	int index;
	int delay;
	Point point;
} PointTask;

void install_point(void* addr, int index, Point* point);
void invalidate_point(void* addr, int index);

void show_task(void *task);
void show_points(void* shmaddr, int max);
