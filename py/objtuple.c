#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "nlr.h"
#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "runtime0.h"
#include "runtime.h"
#include "objtuple.h"

static mp_obj_t mp_obj_new_tuple_iterator(mp_obj_tuple_t *tuple, int cur);

/******************************************************************************/
/* tuple                                                                      */

void tuple_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t o_in, mp_print_kind_t kind) {
    mp_obj_tuple_t *o = o_in;
    print(env, "(");
    for (int i = 0; i < o->len; i++) {
        if (i > 0) {
            print(env, ", ");
        }
        mp_obj_print_helper(print, env, o->items[i], PRINT_REPR);
    }
    if (o->len == 1) {
        print(env, ",");
    }
    print(env, ")");
}

static mp_obj_t tuple_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    // TODO check n_kw == 0

    switch (n_args) {
        case 0:
            // return a empty tuple
            return mp_const_empty_tuple;

        case 1:
        {
            // 1 argument, an iterable from which we make a new tuple
            if (MP_OBJ_IS_TYPE(args[0], &tuple_type)) {
                return args[0];
            }

            // TODO optimise for cases where we know the length of the iterator

            uint alloc = 4;
            uint len = 0;
            mp_obj_t *items = m_new(mp_obj_t, alloc);

            mp_obj_t iterable = rt_getiter(args[0]);
            mp_obj_t item;
            while ((item = rt_iternext(iterable)) != mp_const_stop_iteration) {
                if (len >= alloc) {
                    items = m_renew(mp_obj_t, items, alloc, alloc * 2);
                    alloc *= 2;
                }
                items[len++] = item;
            }

            mp_obj_t tuple = mp_obj_new_tuple(len, items);
            m_free(items, alloc);

            return tuple;
        }

        default:
            nlr_jump(mp_obj_new_exception_msg_1_arg(MP_QSTR_TypeError, "tuple takes at most 1 argument, %d given", (void*)(machine_int_t)n_args));
    }
}

static mp_obj_t tuple_unary_op(int op, mp_obj_t self_in) {
    mp_obj_tuple_t *self = self_in;
    switch (op) {
        case RT_UNARY_OP_BOOL: return MP_BOOL(self->len != 0);
        case RT_UNARY_OP_LEN: return MP_OBJ_NEW_SMALL_INT(self->len);
        default: return MP_OBJ_NULL; // op not supported for None
    }
}

static mp_obj_t tuple_binary_op(int op, mp_obj_t lhs, mp_obj_t rhs) {
    mp_obj_tuple_t *o = lhs;
    switch (op) {
        case RT_BINARY_OP_SUBSCR:
        {
            // tuple load
            uint index = mp_get_index(o->base.type, o->len, rhs);
            return o->items[index];
        }
        default:
            // op not supported
            return NULL;
    }
}

static mp_obj_t tuple_getiter(mp_obj_t o_in) {
    return mp_obj_new_tuple_iterator(o_in, 0);
}

const mp_obj_type_t tuple_type = {
    { &mp_const_type },
    "tuple",
    .print = tuple_print,
    .make_new = tuple_make_new,
    .unary_op = tuple_unary_op,
    .binary_op = tuple_binary_op,
    .getiter = tuple_getiter,
};

// the zero-length tuple
static const mp_obj_tuple_t empty_tuple_obj = {{&tuple_type}, 0};
const mp_obj_t mp_const_empty_tuple = (mp_obj_t)&empty_tuple_obj;

mp_obj_t mp_obj_new_tuple(uint n, const mp_obj_t *items) {
    if (n == 0) {
        return mp_const_empty_tuple;
    }
    mp_obj_tuple_t *o = m_new_obj_var(mp_obj_tuple_t, mp_obj_t, n);
    o->base.type = &tuple_type;
    o->len = n;
    if (items) {
        for (int i = 0; i < n; i++) {
            o->items[i] = items[i];
        }
    }
    return o;
}

void mp_obj_tuple_get(mp_obj_t self_in, uint *len, mp_obj_t **items) {
    assert(MP_OBJ_IS_TYPE(self_in, &tuple_type));
    mp_obj_tuple_t *self = self_in;
    if (len) {
        *len = self->len;
    }
    if (items) {
        *items = &self->items[0];
    }
}

void mp_obj_tuple_del(mp_obj_t self_in) {
    assert(MP_OBJ_IS_TYPE(self_in, &tuple_type));
    mp_obj_tuple_t *self = self_in;
    m_del_var(mp_obj_tuple_t, mp_obj_t, self->len, self);
}

/******************************************************************************/
/* tuple iterator                                                             */

typedef struct _mp_obj_tuple_it_t {
    mp_obj_base_t base;
    mp_obj_tuple_t *tuple;
    machine_uint_t cur;
} mp_obj_tuple_it_t;

static mp_obj_t tuple_it_iternext(mp_obj_t self_in) {
    mp_obj_tuple_it_t *self = self_in;
    if (self->cur < self->tuple->len) {
        mp_obj_t o_out = self->tuple->items[self->cur];
        self->cur += 1;
        return o_out;
    } else {
        return mp_const_stop_iteration;
    }
}

static const mp_obj_type_t tuple_it_type = {
    { &mp_const_type },
    "tuple_iterator",
    .iternext = tuple_it_iternext,
};

static mp_obj_t mp_obj_new_tuple_iterator(mp_obj_tuple_t *tuple, int cur) {
    mp_obj_tuple_it_t *o = m_new_obj(mp_obj_tuple_it_t);
    o->base.type = &tuple_it_type;
    o->tuple = tuple;
    o->cur = cur;
    return o;
}
