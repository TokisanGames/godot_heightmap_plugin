#include "quad_tree_lod.h"

namespace godot {

QuadTreeLod::QuadTreeLod() {
	Godot::print("QuadTreeLod: GDNative starting up");
    _tree = new Quad();
}

QuadTreeLod::~QuadTreeLod() {
	Godot::print("QuadTreeLod: GDNative deleting QuadTreeLod");
    delete _tree;
}

void QuadTreeLod::_register_methods() {
	register_method("set_callbacks", &QuadTreeLod::set_callbacks);
	register_method("clear", &QuadTreeLod::clear);
	register_method("compute_lod_count", &QuadTreeLod::compute_lod_count);
	register_method("get_lod_count", &QuadTreeLod::get_lod_count);
	register_method("get_lod_factor", &QuadTreeLod::get_lod_factor);
	register_method("create_from_sizes", &QuadTreeLod::create_from_sizes);
	register_method("set_split_scale", &QuadTreeLod::set_split_scale);
	register_method("get_split_scale", &QuadTreeLod::get_split_scale);
	register_method("update", &QuadTreeLod::update);
	register_method("debug_draw_tree", &QuadTreeLod::debug_draw_tree);
}

void QuadTreeLod::_init() {
    Godot::print("QuadTreeLod: _init()");
}

void QuadTreeLod::set_callbacks(Ref<FuncRef> make_cb, Ref<FuncRef> recycle_cb, Ref<FuncRef> vbounds_cb) {
    Godot::print("set_callbacks: make_cb->get_function: " + make_cb->get_function());
	_make_func = make_cb;
	_recycle_func = recycle_cb;
	_vertical_bounds_func = vbounds_cb;
}

void QuadTreeLod::clear() {
	_join_all_recursively(_tree, _max_depth);
	_max_depth = 0;
	_base_size = 0;
}

int QuadTreeLod::compute_lod_count(int base_size, int full_size) {
	int po = 0;
	while (full_size > base_size) {
		full_size = full_size >> 1;
		po += 1;
	}
	return po;
}

int QuadTreeLod::get_lod_count() {
	// TODO make this a count, not max
	return _max_depth + 1;
}

int QuadTreeLod::get_lod_factor(int lod) {
	return 1 << lod;
}

void QuadTreeLod::create_from_sizes(int base_size, int full_size) {
	clear();
	_base_size = base_size;
	_max_depth = compute_lod_count(base_size, full_size);
}

// The higher, the longer LODs will spread and higher the quality.
// The lower, the shorter LODs will spread and lower the quality.
void QuadTreeLod::set_split_scale(real_t p_split_scale) {
	real_t MIN = 2.0;
	real_t MAX = 5.0;

	// Split scale must be greater than a threshold,
	// otherwise lods will decimate too fast and it will look messy
	if (p_split_scale < MIN)
		p_split_scale = MIN;
	if (p_split_scale > MAX)
		p_split_scale = MAX;

	_split_scale = p_split_scale;
}

real_t QuadTreeLod::get_split_scale() {
	return _split_scale;
}

void QuadTreeLod::update(Vector3 view_pos) {
	_update(_tree, _max_depth, view_pos);

	// This makes sure we keep seeing the lowest LOD,
	// if the tree is cleared while we are far away
	if (! _tree->has_children() && _tree->data.get_type() == Variant::NIL)
		_tree->data = _make_chunk(_max_depth, 0, 0);
}

void QuadTreeLod::_update(Quad *quad, int lod, Vector3 view_pos) {
    // This function should be called regularly over frames.

    int lod_factor = get_lod_factor(lod);
    int chunk_size = _base_size * lod_factor;
    Vector3 world_center = (real_t)chunk_size * (Vector3((real_t)quad->origin_x, 0, (real_t)quad->origin_y) + Vector3(0.5, 0, 0.5));

    if (_vertical_bounds_func.is_valid()) {
        Variant ret = _vertical_bounds_func->call_func(quad->origin_x, quad->origin_y, lod);
        Vector2 vbounds;
        if (ret.get_type() == Variant::VECTOR2)
            vbounds = (Vector2)ret;
        else
            Godot::print("Error: QuadtreeLod::_update: _vertical_bounds_func did not return a vector2");
        world_center.y = (vbounds.x + vbounds.y) / 2.0;
    }

    int split_distance = _base_size * lod_factor * _split_scale;

    if (! quad->has_children()) {
        if (lod > 0 && world_center.distance_to(view_pos) < split_distance) {
            // Split
            quad->add_children();

            for (int i = 0; i < 4; i++) {
                Quad* child = new Quad();
                child->origin_x = quad->origin_x * 2 + (i & 1);
                child->origin_y = quad->origin_y * 2 + ((i & 2) >> 1);
                child->data = _make_chunk(lod - 1, child->origin_x, child->origin_y);
                quad->set_child(i, child);
                // If the quad needs to split more, we'll ask more recycling...
            }

            if (quad->data.get_type() == Variant::OBJECT) {
                _recycle_chunk(quad->data, quad->origin_x, quad->origin_y, lod);
                quad->data = Variant();
            }
        }
    } else {
        bool no_split_child = true;

        for (int i = 0; i < 4; i++) {
            _update(quad->get_child(i), lod - 1, view_pos);
            if (quad->get_child(i)->has_children())
                no_split_child = false;
        }

        if (no_split_child && world_center.distance_to(view_pos) > split_distance) {
            // Join
            if (quad->has_children()) {
                for (int i = 0; i < 4; i++) {
                    Quad* child = quad->get_child(i);
                    _recycle_chunk(child->data, child->origin_x, child->origin_y, lod - 1);
                    quad->data = Variant();
                }
                quad->clear_children();

                //assert(quad->data == null);
                //if ( quad->data.is_valid() )    // Really necessary after it was just unrefed in quad?
                	//Godot::print("QuadTreeLod error: quad->data is still valid");

                quad->data = _make_chunk(lod, quad->origin_x, quad->origin_y);
            }
        }
    }
} // _update

void QuadTreeLod::_join_all_recursively(Quad *quad, int lod) {
    if (quad->has_children()) {
        for (int i = 0; i < 4; i++) {
            _join_all_recursively(quad->get_child(i), lod - 1);
        }
        quad->clear_children();

    } else if (quad->data.get_type() == Variant::OBJECT) {
        _recycle_chunk(quad->data, quad->origin_x, quad->origin_y, lod);
        quad->data = Variant();
    }
}

Variant QuadTreeLod::_make_chunk(int lod, int origin_x, int origin_y) {
    if (_make_func.is_valid())
        return _make_func->call_func(origin_x, origin_y, lod);
    else
        return Variant();
}

void QuadTreeLod::_recycle_chunk(Variant chunk, int origin_x, int origin_y, int lod) {
    if (_recycle_func.is_valid())
        _recycle_func->call_func(chunk, origin_x, origin_y, lod);
}

void QuadTreeLod::debug_draw_tree(Variant ci) {
    if (ci.get_type() == Variant::OBJECT && ci.has_method("draw_rect"))
        _debug_draw_tree_recursive(ci, _tree, _max_depth, 0);
}

void QuadTreeLod::_debug_draw_tree_recursive(Variant ci, Quad *quad, int lod_index, int child_index) {
    if (quad->has_children()) {
        for (int i = 0; i < 4; i++) {
            _debug_draw_tree_recursive(ci, quad->get_child(i), lod_index - 1, i);
        }

    } else {
        int size = get_lod_factor(lod_index);
        int checker = 0;
        if (child_index == 1 || child_index == 2)
            checker = 1;

        int chunk_indicator = 0;
        if (quad->data.get_type() == Variant::OBJECT)
            chunk_indicator = 1;

        Variant rect2(Rect2( Vector2((real_t)quad->origin_x, (real_t)quad->origin_y) * (real_t)size, 
                             Vector2((real_t)size, (real_t)size)));
        Variant color(Color(1.0 - (real_t)lod_index * 0.2, 0.2 * (real_t)checker, (real_t)chunk_indicator, 1));
        Variant *args[] = { &rect2, &color };
        ci.call("draw_rect", (const Variant **)args, 2);
    }
}

} // namespace godot
