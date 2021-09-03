#ifndef QUAD_TREE_LOD_H
#define QUAD_TREE_LOD_H

#include <Godot.hpp>
#include <FuncRef.hpp>

namespace godot {

class QuadTreeLod : public Reference {
    GODOT_CLASS(QuadTreeLod, Reference)

    class Quad {
    private:
        Quad **children = nullptr;
        bool _has_children = false;

    public:
        int origin_x = 0;
        int origin_y = 0;
        Variant data;          // Type is HTerrainChunk.gd

        Quad() {
            //Always allocate memory for child container, but not children
            children = new Quad* [4];
            for (int i = 0; i < 4; i++)
                children[i] = nullptr;
        }

        ~Quad() {
            clear_children();
            delete[] children;
        }

        void clear() {
            clear_children();
            data = Variant();
        }

        void add_children() {
            if (_has_children)
                Godot::print("Error: Quad::add_children(): already has children");
                return;

            for (int i = 0; i < 4; i++)
                children[i] = new Quad();
            _has_children = true;
        }

        void clear_children() {
            if (_has_children) {
                for (int i = 0; i < 4; i++) {
                    delete children[i];
                    children[i] = nullptr;
                }
            }
            _has_children = false;
        }

        bool has_children() {
            return _has_children;
        }

        Quad* get_child(int idx) {
            return children[idx];
        }

        void set_child(int idx, Quad *child) {
            children[idx] = child;
        }
    };

private:
    Quad *_tree;
    int _max_depth = 0;
    int _base_size = 16;
    real_t _split_scale = 2.0;

    Ref<FuncRef> _make_func;
    Ref<FuncRef> _recycle_func;
    Ref<FuncRef> _vertical_bounds_func;

public:

    QuadTreeLod();
    ~QuadTreeLod();

    static void _register_methods();

    void _init();

    void set_callbacks(Ref<FuncRef> make_cb, Ref<FuncRef> recycle_cb, Ref<FuncRef> vbounds_cb);

    void clear();

    /*static*/ int compute_lod_count(int base_size, int full_size);
    int get_lod_count();
    int get_lod_factor(int lod);

    void create_from_sizes(int base_size, int full_size);

    void set_split_scale(real_t p_split_scale);
    real_t get_split_scale();

    void update(Vector3 view_pos);

    void debug_draw_tree(Variant ci);   // CanvasItem

private:

    void _update(Quad *quad, int lod, Vector3 view_pos);
    void _join_all_recursively(Quad *quad, int lod);

    Variant _make_chunk(int lod, int origin_x, int origin_y);
    void _recycle_chunk(Variant chunk, int origin_x, int origin_y, int lod); // Chunk:HTerrainChunk.gd

    void _debug_draw_tree_recursive(Variant ci, Quad *quad, int lod_index, int child_index);

}; // class QuadTreeLod

} // namespace godot

#endif // QUAD_TREE_LOD_H