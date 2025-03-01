#include "util/types.h"
#include "dmath.h"
#include <algorithm>

func bitonic_merge(u32 *array, u32 low_index, u32 count, u8 direction) {
	if(count <= 1) return;

	u32 k = count / 2;

	for(u32 i = low_index; i < low_index + k; ++i) {
		if(direction == (array[i] > array[i + k])) {
			u32 temp = array[i];
			array[i] = array[i + k];
			array[i + k] = temp;
		}
	}

	bitonic_merge(array, low_index, k, direction);
	bitonic_merge(array, low_index + k, k, direction);
}

func bitonic_sort(u32 *array, u32 low_index, u32 count, u8 direction) {
	if(count <= 1) return;

	u32 k = count / 2;

	bitonic_sort(array, low_index, k, 1);
	bitonic_sort(array, low_index + k, k, 0);
	bitonic_merge(array, low_index, count, direction);

	// auto pred = [](u32 a, u32 b){
	// 	return (a < b);
	// };

	// std::sort(array, array + count, pred);
}