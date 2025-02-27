struct Entry {
	uint original_index;
	uint hash;
	uint key;
};

struct Info {
	uint num_entries;
	uint group_width;
	uint group_height;
	uint step_index;
	uint num_per_dispatch;
};

RWStructuredBuffer <Entry> Entries : register(u4);
RWStructuredBuffer <Info> Infos : register(u0);
RWBuffer<uint> Offsets : register(u5);

// cbuffer parameters : register(b0) {
// 	uint num_entries;
// 	uint group_width;
// 	uint group_height;
// 	uint step_index;
// 	uint padding[15];
// };


// Sorting entries by their keys (smallest to largest). This is done by bitonic merge sort
[numthreads(128, 1, 1)]
void Sort(uint3 id : SV_DispatchThreadID, uint3 group_id : SV_GroupID) {
	uint i = id.x;

	uint group_width 	= Infos[id.x * group_id.x / Infos[0].num_per_dispatch].group_width;
	uint group_height 	= Infos[id.x * group_id.x / Infos[0].num_per_dispatch].group_height;
	uint step_index 	= Infos[id.x * group_id.x / Infos[0].num_per_dispatch].step_index;
	uint num_entries 	= Infos[id.x * group_id.x / Infos[0].num_per_dispatch].num_entries;

	uint h_index = i & (group_width - 1);
	uint index_left = h_index + (group_height + 1) * (i / group_width);
	uint right_step_size = step_index == 0 ? group_height - 2 * h_index : (group_height + 1) / 2;
	uint index_right = index_left + right_step_size;

	if(index_right >= num_entries) return;

	uint value_left = Entries[index_left].key;
	uint value_right = Entries[index_right].key;

	if(value_left > value_right) {
		Entry temp = Entries[index_left];
		Entries[index_left] = Entries[index_right];
		Entries[index_right] = temp;
	}
}

[numthreads(128, 1, 1)]
void Calculate_Offsets(uint3 id : SV_DispatchThreadID) {
	uint num_entries = Infos[0].num_entries;

	if(id.x >= num_entries) return;

	uint i = id.x;
	uint null = num_entries;

	uint key = Entries[i].key;
	uint key_prev = i == 0 ? null : Entries[i - 1].key;

	if(key != key_prev) {
		Offsets[key] = i;
	}
}