struct Entry {
	uint original_index;
	uint hash;
	uint key;
};

RWStructuredBuffer <Entry> Entries : register(u0);

cbuffer parameters : register(b0) {
	uint num_entries;
	uint group_width;
	uint group_height;
	uint step_index;
	uint padding[15];
};


// Sorting entries by their keys (smallest to largest). This is done by bitonic merge sort
[numthreads(128, 1, 1)]
void Sort(uint3 id : SV_DispatchThreadID) {
	uint i = id.x;

	uint h_index = i & (group_width - 1);
	uint index_left = h_index + (group_height + 1) * (i / group_width);
	uint right_step_size = step_index == 0 ? group_height - 2 * h_index : (group_height + 1) / 2;
	uint index_right = index_left + right_step_size;

	if(index_right >= num_entries) return;

	uint value_left = Entries[index_left];
	uint value_right = Entries[index_right];

	if(value_left > value_right) {
		Entry temp = Entries[index_left];
		Entries[index_left] = Entries[index_right];
		Entries[index_right] = temp;
	}
}

RWBuffer<uint> Offsets;

[numthreads(128, 1, 1)]
void Calculate_Offsets(uint3 id : SV_DispatchThreadID) {
	if(id.x >= num_entries) return;

	uint i = id.x;
	uint null = num_entries;

	uint key = Entries[i].key;
	uint key_prev = i == 0 ? null : Entries[i - 1].key;

	if(key != key_prev) {
		Offsets[key] = i;
	}
}