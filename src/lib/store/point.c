/*
 * Library: store - a generic set of storage data structures
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "log_mgr.h"
#include "point.h"


void show_task(void *task) {
	log_event(WARNING, " ● Task(idx=%d, delay=%d, Point(is_valid=%d, x=%2.3f, y=%2.3f))",
						((PointTask *)task)->index,
						((PointTask *)task)->delay,
						((PointTask *)task)->point.is_valid,
						((PointTask *)task)->point.x,
						((PointTask *)task)->point.y);
}

void show_points(void* shmaddr, int max) {
	/* REQ_monitor_3: Approximately each second, this program will print a line
	of information to the screen about the contents of the shared memory.
	Information to be reported includes:

	-   Count of the active array elements (i.e. the number of elements which are valid)
	-   The average x value over the active array elements, and
	-   The average y value over the active array elements.
	*/

	int idx, valid_points = 0;
	float sum_x = 0, sum_y = 0;

	/* collect data and determine stats */
	for (idx=0; idx < max; idx++){
		Point *point = &((Point*) shmaddr)[idx];
		if (point->is_valid == 1){
			valid_points += 1;
			sum_x += point->x;
			sum_y += point->y;
		}
	}

	/* display stats */
	if (valid_points > 0) {
		log_event(WARNING, " ● PointStats(valid_count=%d, avg_x=%2.3f, avg_y=%2.3f)",
												valid_points,
												(sum_x/valid_points),
												(sum_y/valid_points));

		/* print each point out as well... i chose not to use iterate_list to make
		formatting of the list to look nicer */
		for (idx=0; idx < max; idx++){
			Point *point = &((Point*) shmaddr)[idx];
			if (point->is_valid == 1){
				valid_points -= 1;
				if (valid_points == 0) {
					log_event(WARNING, "   └── Idx:%d = Point(is_valid=%d, x=%2.3f, y=%2.3f)",
															idx,
															point->is_valid,
															point->x,
															point->y);
				} else {
					log_event(WARNING, "   ├── Idx:%d = Point(is_valid=%d, x=%2.3f, y=%2.3f)",
															idx,
															point->is_valid,
															point->x,
															point->y);
				}
			}
		}

	} else {
		log_event(WARNING, " ● PointStats(valid_count=0, avg_x=0, avg_y=0)");
	}
}

void install_point(void* addr, int index, Point* point) {
	log_event(INFO, " Installing new point (index:%d)", index);
	if (index < 0 || index >= MAX_NUM_POINTS) {
		log_event(FATAL, " Error: invalid point index (%d). Cancelling point installation.", index);
	} else {
	  memcpy(&(((Point*) addr)[index]), point, sizeof(Point));
	}
}

void invalidate_point(void* addr, int index) {
	log_event(INFO, " Invalidating existing point (index:%d)", index);
	if (index < 0 || index >= MAX_NUM_POINTS) {
		log_event(FATAL, " Error: invalid point index (%d). Cancelling point invalidation.", index);
	} else {
	  (((Point*) addr)[index]).is_valid = 0;
	}
}
