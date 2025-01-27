#include <cstdlib>
#include <stdio.h>
#include <vector>

struct type {

	enum tag_t {
		i32 = 0,
		f32,
		vec2f,
		vec3f,
		vec4f,
		array,
		structure,
	};

	struct field {

	};

	tag_t tag;

	union {
		struct {int size; type* ty;} array;
		struct {int field_count; type* field_types;} structure;
	} g;
};

struct term {
	enum tag_t {
		vert_shader = 0,
		frag_shader,

		f32_val,

		i32_val,
		i32_add,
		i32_sub,
		i32_mul,
		i32_div,

		vec4f_val,

		structure_val,
		structure_proj,
        var,
	};

	tag_t tag;

	union {
		struct {term* e_pos; term* e_uv;} 	vert_shader;
		struct {term* e_color;} 			frag_shader;
		
		struct {int idx;}			var;
		
		struct {float i;} 			 f32_val;
		
		struct {int i;} 			 i32_val;
		struct {term* e1; term *e2;} i32_add;
		struct {term* e1; term *e2;} i32_sub;
		struct {term* e1; term *e2;} i32_mul;
		struct {term* e1; term *e2;} i32_div;

		struct {term* e1; term *e2; term *e3; term *e4;} vec4f_val;
		
		struct {int field_count; term* fields;} structure_val;
		struct {term* e; int idx;} 				structure_proj;
	} g;

	int node_idx;
};

struct inferrence_result {
	bool success;
	type ty;
};

auto infer(std::vector<type*> ctx, term* _e) -> inferrence_result {
	auto e = *_e;

	switch(e.tag) {
        case term::var: {
            if(e.g.var.idx < ctx.size())
            {
				auto ty = ctx[e.g.var.idx];
                return inferrence_result {true, *ty};
            } else {
				return inferrence_result {false, {}};
			}
        }; break;
		case term::i32_val: {return inferrence_result {true, type {type::i32, {}}};}; break;
		case term::structure_val: {
			auto el = e.g.structure_val;
			
			type ty = {
				type::structure,
				{.structure = {
					.field_count = el.field_count, 
					.field_types = (type*)malloc(sizeof(type) * el.field_count)
				}}
			};

			for(int i = 0; i != e.g.structure_val.field_count; ++i)
			{
				auto inf = infer(ctx, &el.fields[i]);
				if(inf.success) {
					ty.g.structure.field_types[i] = inf.ty;
				} else {
					return inferrence_result {false, {}};
				}
			}

			return inferrence_result {true, ty};
		}; break;
		default: printf("%s\n", "internal error"); exit(-1);
	}
}

auto check(std::vector<type*> ctx, term* _e, type ty) -> bool {
	auto e = *_e;
	switch(e.tag) {
		case term::i32_val: if (ty.tag == type::i32) {return true;}
							else if(ty.tag == type::f32) {return false;}
							else {return false;}; break;
		case term::i32_add: if (ty.tag == type::i32) {return check(ctx, e.g.i32_add.e1, type {type::i32, {}}) && check(ctx, e.g.i32_add.e2, type {type::i32, {}});} else {return false;}; break;
		case term::i32_sub: if (ty.tag == type::i32) {return check(ctx, e.g.i32_sub.e1, type {type::i32, {}}) && check(ctx, e.g.i32_sub.e2, type {type::i32, {}});} else {return false;}; break;
		case term::i32_mul: if (ty.tag == type::i32) {return check(ctx, e.g.i32_mul.e1, type {type::i32, {}}) && check(ctx, e.g.i32_mul.e2, type {type::i32, {}});} else {return false;}; break;
		case term::i32_div: if (ty.tag == type::i32) {return check(ctx, e.g.i32_div.e1, type {type::i32, {}}) && check(ctx, e.g.i32_div.e2, type {type::i32, {}});} else {return false;}; break;
		case term::vec4f_val: 
			if (ty.tag == type::vec4f) {
				return check(ctx, e.g.vec4f_val.e1, type {type::f32, {}}) 
					&& check(ctx, e.g.vec4f_val.e2, type {type::f32, {}})
					&& check(ctx, e.g.vec4f_val.e3, type {type::f32, {}})
					&& check(ctx, e.g.vec4f_val.e4, type {type::f32, {}});
			} else {return false;}; 
		break;
		case term::structure_val: 
			if(ty.tag == type::structure) {
				for(int i = 0; i != ty.g.structure.field_count; ++i) { 
					if(check(ctx, &e.g.structure_val.fields[i], ty.g.structure.field_types[i]) == false) return false;
					continue;
				}
				return true;
			} else {return false;};
		break;
		case term::structure_proj: {
			auto inf = infer(ctx, e.g.structure_proj.e);
			if(inf.ty.tag == type::structure) {
				if(inf.ty.g.structure.field_types[e.g.structure_proj.idx].tag == ty.tag) { return true; } 
				else {return false;}
			} else {return false;}
		}; break;
		default: {

		};
		//printf("%s\n", "internal error"); exit(-1);
	}
};

auto check_vert_shader(term* _e) {
	auto e = *_e;

	if(e.tag == term::vert_shader) {
		return check(std::vector<type*>(), e.g.vert_shader.e_pos, type {type::vec3f, {}}) && check(std::vector<type*>(), e.g.vert_shader.e_uv, type {type::vec2f, {}});
	} else {
		return false;
	}
}

auto check_frag_shader(std::vector<type*> ctx, term* _e) {
	auto e = *_e;

	if(e.tag == term::frag_shader) {
        auto ctx = std::vector<type*>();
        auto input_ty = type {type::structure, {.structure = {.field_count = 2, .field_types = (type*)malloc(sizeof(type)*2)}}};
        input_ty.g.structure.field_types[0] = type {type::vec4f, {}};
        input_ty.g.structure.field_types[1] = type {type::vec2f, {}};
        
        ctx.push_back(&input_ty);
        auto chk = check(ctx, e.g.vert_shader.e_pos, type {type::vec3f, {}}) && check(std::vector<type*>(), e.g.vert_shader.e_uv, type {type::vec2f, {}});
        ctx.pop_back();
		return chk;
	} else {
		return false;
	}
}

auto generate_hlsl_code(char* out_buf, term* _e) -> char* {
	auto e = *_e;

	switch (e.tag) {
		case term::frag_shader: {
			out_buf += sprintf(out_buf, "float4 PSMain(PSInput input) : SV_TARGET {\nreturn");
			out_buf = generate_hlsl_code(out_buf, e.g.frag_shader.e_color);
		}; break;
		case term::var: {
			out_buf += sprintf(out_buf, "x_%i", e.g.var.idx);
			return out_buf;
		}; break;
		case term::i32_val: {
			out_buf += sprintf(out_buf, "%i", e.g.i32_val.i);
			return out_buf;
		}; break;
		case term::i32_add: 	{
			out_buf = generate_hlsl_code(out_buf, e.g.i32_add.e1);
			out_buf += sprintf(out_buf, "%s", " + ");
			out_buf = generate_hlsl_code(out_buf, e.g.i32_add.e2);
			return out_buf;
		}; break;
		case term::i32_mul: 	{
			out_buf = generate_hlsl_code(out_buf, e.g.i32_mul.e1);
			out_buf += sprintf(out_buf, "%s", " * ");
			out_buf = generate_hlsl_code(out_buf, e.g.i32_mul.e2);
			return out_buf;
		}; break;
		case term::i32_sub: 	{
			out_buf = generate_hlsl_code(out_buf, e.g.i32_mul.e1);
			out_buf += sprintf(out_buf, "%s", " - ");
			out_buf = generate_hlsl_code(out_buf, e.g.i32_mul.e2);
			return out_buf;
		}; break;
		case term::i32_div: 	{
			out_buf = generate_hlsl_code(out_buf, e.g.i32_mul.e1);
			out_buf += sprintf(out_buf, "%s", " / ");
			out_buf = generate_hlsl_code(out_buf, e.g.i32_mul.e2);
			return out_buf;
		}; break;
	}
}

int main()
{
	term e1 = term {term::i32_val, {.i32_val = {11}}};
	term e2 = term {term::i32_val, {.i32_val = {22}}};
	term e = term {term::i32_add, {.i32_add = {&e1, &e2}}};

	char out_buf[4096];
	if(check_vert_shader(&e))
	{
		char* out_buf_end = generate_hlsl_code(out_buf, &e);
		*out_buf_end = 0;
	}
	else {
		printf("This is not a shader!");
	}

	printf("%s\n",out_buf);
}