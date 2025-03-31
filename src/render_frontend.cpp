// TODO(DH):
// For render something we need to create:
// 1. Alocate descriptor heaps (for all resources)
// 2. Resources / Buffers
// 3. Root signatures
// 4. Render pipeline
// 5. Bind resources to pipeline
// 6. Record proper commands to command list\lists (or chain them)
// 7. Execute command list
// Remember command lists and not build them every frame when it's not needed
// this will save some time on CPU

// NOTE(DH): How i imagine how code should look like

ID3D12Resource* 		resources		[...];
ID3D12CommandList*		command_lists	[...];
ID3D12DescriptorHeap*	descriptor_heaps[...];

// Allocate descriptor heaps
// 1. We need to know which resources will be on this heap allocated
desc_heap = create_desc_heap(TYPE_OF_RESOURCES, COUNT, );

// Create resource
resource = create_resource(TEXTURE2D, R32FLOAT, READ, 1024, 1024);
resource = create_resource(TEXTURE1D, R32FLOAT, READ, 1024);
resource = create_resource(BUFFER2D, R8G8B8A8F, READ_WRITE, 512, 512);
resource = create_resource(BUFFER1D, R32FLOAT, READ, 1024);

// Pipeline
pipeline = build_pipeline(PIPELINE_TYPE, SHADER, RESOURCES_FOR_SHADER)

// Pipeline stage
pipeline_stage = build_pipeline_stage(COMPUTE, COMPILED_SHADER/S, RESOURCES)

// Command buffer/bundle building
command_list = build_command_list_for(PIPELINE_STAGE)